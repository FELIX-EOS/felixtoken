#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <string>

namespace eosiosystem
{
class system_contract;
}

namespace felix
{

using std::string;
using namespace eosio;

/**
 * @defgroup eosiotoken eosio.token
 * @ingroup eosiocontracts
 *
 * eosio.token contract
 *
 * @details eosio.token contract defines the structures and actions that allow users to create, issue, and manage
 * tokens on eosio based blockchains.
 * @{
 */
class [[eosio::contract( "felixtoken" )]] token : public contract
{
private:
	static constexpr uint32_t refund_delay_sec = 15 * 24 * 3600;
	// static constexpr uint32_t refund_delay_sec = 600;

public:
	using contract::contract;
	token( name s, name code, datastream<const char *> ds );
	~token();

	ACTION create( const name &issuer, const asset &maximum_supply );
	ACTION issue( const name &to, const asset &quantity, const string &memo );
	ACTION retire( const asset &quantity, const string &memo );
	ACTION transfer( const name &from, const name &to, const asset &quantity, const string &memo );
	ACTION open( const name &owner, const symbol &symbol, const name &ram_payer );
	ACTION close( const name &owner, const symbol &symbol );
	ACTION stake( const name &account, const asset &quantity );
	ACTION unstake( const name &account, const asset &quantity );
	ACTION restake( const name &account );
	ACTION refunddef( const name &account );
	ACTION refund( const name &account );
	ACTION dividend( const name &account, const asset &baseAmount, const asset &totalAmount );
	ACTION banish( const name &account );
	ACTION recalc();

	[[eosio::on_notify( "eosio::onerror" )]] void on_error( const eosio::onerror &error );

	ACTION resend( eosio::transaction trx, uint128_t sender_id );
	using resend_action = action_wrapper<"resend"_n, &token::resend>;

	static asset get_supply( const name &token_contract_account, const symbol_code &sym_code )
	{
		stats statstable( token_contract_account, sym_code.raw() );
		const auto &st = statstable.get( sym_code.raw() );
		return st.supply;
	}

	static asset get_balance( const name &token_contract_account, const name &owner, const symbol_code &sym_code )
	{
		accounts accountstable( token_contract_account, owner.value );
		const auto &ac = accountstable.get( sym_code.raw() );
		return ac.balance;
	}

	using create_action = eosio::action_wrapper<"create"_n, &token::create>;
	using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
	using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
	using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
	using open_action = eosio::action_wrapper<"open"_n, &token::open>;
	using close_action = eosio::action_wrapper<"close"_n, &token::close>;
	using refund_action = eosio::action_wrapper<"refund"_n, &token::refund>;
	using refunddef_action = eosio::action_wrapper<"refunddef"_n, &token::refund>;

private:
	TABLE account
	{
		asset balance;
		uint64_t primary_key() const { return balance.symbol.code().raw(); }
	};

	TABLE currency_stats
	{
		asset supply;
		asset max_supply;
		name issuer;

		uint64_t primary_key() const { return supply.symbol.code().raw(); }
	};

	TABLE staking
	{
		name account;
		asset quantity;

		uint64_t primary_key() const { return account.value; }
	};

	TABLE unstaking
	{
		name account;
		asset quantity;
		time_point req_time;

		uint64_t primary_key() const { return account.value; }
	};

	TABLE totalstake { asset quantity; };

	typedef eosio::multi_index<"accounts"_n, account> accounts;
	typedef eosio::multi_index<"stat"_n, currency_stats> stats;
	typedef eosio::multi_index<"staking"_n, staking> staking_index;
	typedef eosio::multi_index<"unstaking"_n, unstaking> unstaking_index;
	typedef eosio::singleton<"totalstake"_n, totalstake> totalstake_index;

	totalstake _totalStake;
	totalstake_index _totalStakeSingleton;

	void sub_balance( const name &owner, const asset &value );
	void add_balance( const name &owner, const asset &value, const name &ram_payer );
	void _refund( const name &account );
};
/** @}*/ // end of @defgroup eosiotoken eosio.token
} // namespace felix
