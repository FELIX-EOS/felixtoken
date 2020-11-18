#include "../include/felixtoken.hpp"
#include <eosio/transaction.hpp>

namespace felix
{

using namespace eosio;
const symbol felix_symbol( "FLX", 4 );
const symbol eos_symbol( "EOS", 4 );

token::token( name s, name code, datastream<const char *> ds )
	: contract( s, code, ds )
	, _totalStakeSingleton( get_self(), get_self().value )
{
	_totalStake = _totalStakeSingleton.exists() ? _totalStakeSingleton.get() : totalstake{ asset( 0, felix_symbol ) };
}

token::~token() { _totalStakeSingleton.set( _totalStake, get_self() ); }

void token::create( const name &issuer, const asset &maximum_supply )
{
	require_auth( get_self() );

	auto sym = maximum_supply.symbol;
	check( sym.is_valid(), "invalid symbol name" );
	check( maximum_supply.is_valid(), "invalid supply" );
	check( maximum_supply.amount > 0, "max-supply must be positive" );

	stats statstable( get_self(), sym.code().raw() );
	auto existing = statstable.find( sym.code().raw() );
	check( existing == statstable.end(), "token with symbol already exists" );

	statstable.emplace( get_self(), [&]( auto &s ) {
		s.supply.symbol = maximum_supply.symbol;
		s.max_supply = maximum_supply;
		s.issuer = issuer;
	} );
}

void token::issue( const name &to, const asset &quantity, const string &memo )
{
	auto sym = quantity.symbol;
	check( sym.is_valid(), "invalid symbol name" );
	check( memo.size() <= 256, "memo has more than 256 bytes" );

	stats statstable( get_self(), sym.code().raw() );
	auto existing = statstable.find( sym.code().raw() );
	check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
	const auto &st = *existing;
	check( to == st.issuer, "tokens can only be issued to issuer account" );

	require_auth( st.issuer );
	check( quantity.is_valid(), "invalid quantity" );
	check( quantity.amount > 0, "must issue positive quantity" );

	check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
	check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply" );

	statstable.modify( st, same_payer, [&]( auto &s ) { s.supply += quantity; } );

	add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset &quantity, const string &memo )
{
	auto sym = quantity.symbol;
	check( sym.is_valid(), "invalid symbol name" );
	check( memo.size() <= 256, "memo has more than 256 bytes" );

	stats statstable( get_self(), sym.code().raw() );
	auto existing = statstable.find( sym.code().raw() );
	check( existing != statstable.end(), "token with symbol does not exist" );
	const auto &st = *existing;

	require_auth( st.issuer );
	check( quantity.is_valid(), "invalid quantity" );
	check( quantity.amount > 0, "must retire positive quantity" );

	check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

	statstable.modify( st, same_payer, [&]( auto &s ) { s.supply -= quantity; } );

	sub_balance( st.issuer, quantity );
}

void token::transfer( const name &from, const name &to, const asset &quantity, const string &memo )
{
	check( from != to, "cannot transfer to self" );
	require_auth( from );
	check( is_account( to ), "to account does not exist" );
	auto sym = quantity.symbol.code();
	stats statstable( get_self(), sym.raw() );
	const auto &st = statstable.get( sym.raw() );

	require_recipient( from );
	require_recipient( to );

	check( quantity.is_valid(), "invalid quantity" );
	check( quantity.amount > 0, "must transfer positive quantity" );
	check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
	check( memo.size() <= 256, "memo has more than 256 bytes" );

	auto payer = has_auth( to ) ? to : from;

	sub_balance( from, quantity );
	add_balance( to, quantity, payer );
}

void token::sub_balance( const name &owner, const asset &value )
{
	accounts from_acnts( get_self(), owner.value );

	const auto &from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
	check( from.balance.amount >= value.amount, "overdrawn balance" );

	from_acnts.modify( from, owner, [&]( auto &a ) { a.balance -= value; } );
}

void token::add_balance( const name &owner, const asset &value, const name &ram_payer )
{
	accounts to_acnts( get_self(), owner.value );
	auto to = to_acnts.find( value.symbol.code().raw() );
	if ( to == to_acnts.end() )
	{
		to_acnts.emplace( ram_payer, [&]( auto &a ) { a.balance = value; } );
	}
	else
	{
		to_acnts.modify( to, same_payer, [&]( auto &a ) { a.balance += value; } );
	}
}

void token::open( const name &owner, const symbol &symbol, const name &ram_payer )
{
	require_auth( ram_payer );

	check( is_account( owner ), "owner account does not exist" );

	auto sym_code_raw = symbol.code().raw();
	stats statstable( get_self(), sym_code_raw );
	const auto &st = statstable.get( sym_code_raw, "symbol does not exist" );
	check( st.supply.symbol == symbol, "symbol precision mismatch" );

	accounts acnts( get_self(), owner.value );
	auto it = acnts.find( sym_code_raw );
	if ( it == acnts.end() )
	{
		acnts.emplace( ram_payer, [&]( auto &a ) { a.balance = asset{ 0, symbol }; } );
	}
}

void token::close( const name &owner, const symbol &symbol )
{
	require_auth( owner );
	accounts acnts( get_self(), owner.value );
	auto it = acnts.find( symbol.code().raw() );
	check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
	check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
	acnts.erase( it );
}

void token::stake( const name &account, const asset &quantity )
{
	require_auth( account );
	check( quantity.amount > 0, "Stake must positive value" );
	check( quantity.symbol == felix_symbol, "You can only stake FLX token" );

	auto _staking = staking_index( get_self(), get_self().value );
	auto itor = _staking.find( account.value );

	if ( itor == _staking.end() )
	{
		_staking.emplace( get_self(), [&]( auto &a ) {
			a.account = account;
			a.quantity = quantity;
		} );
	}
	else
	{
		_staking.modify( itor, get_self(), [&]( auto &a ) { a.quantity += quantity; } );
	}

	_totalStake.quantity += quantity;

	token::transfer_action( get_self(), { { account, "active"_n } } )
		.send( account, "felixstaking"_n, quantity, "stake" );
}

void token::unstake( const name &account, const asset &quantity )
{
	require_auth( account );
	check( quantity.amount > 0, "Unstake must positive value" );

	auto _staking = staking_index( get_self(), get_self().value );
	auto _unstaking = unstaking_index( get_self(), get_self().value );

	auto stakeItor = _staking.find( account.value );
	auto unstakeItor = _unstaking.find( account.value );

	asset unstaking_amount = asset( 0, felix_symbol );
	if ( unstakeItor != _unstaking.end() ) unstaking_amount += unstakeItor->quantity;

	check( stakeItor != _staking.end(), "No staking account" );
	check( stakeItor->quantity >= quantity, "Not enough staked token" );

	if ( stakeItor->quantity == quantity ) { _staking.erase( stakeItor ); }
	else
	{
		_staking.modify( stakeItor, get_self(), [&]( auto &a ) { a.quantity -= quantity; } );
	}

	if ( unstakeItor == _unstaking.end() )
	{
		_unstaking.emplace( get_self(), [&]( auto &a ) {
			a.account = account;
			a.quantity = quantity;
			a.req_time = current_time_point();
		} );
	}
	else
	{
		_unstaking.modify( unstakeItor, get_self(), [&]( auto &a ) {
			a.quantity += quantity;
			a.req_time = current_time_point();
		} );
	}

	_totalStake.quantity -= quantity;

	eosio::cancel_deferred( account.value ); // TODO: Remove this line when replacing deferred trxs is fixed

	transaction out;
	out.actions.emplace_back( token::refunddef_action( get_self(), { get_self(), "active"_n } ).to_action( account ) );
	out.delay_sec = refund_delay_sec;
	out.send( account.value, get_self() );
}

void token::restake( const name &account )
{
	require_auth( get_self() );
	auto _staking = staking_index( get_self(), get_self().value );
	auto _unstaking = unstaking_index( get_self(), get_self().value );

	auto stakeItor = _staking.find( account.value );
	auto unstakeItor = _unstaking.find( account.value );

	check( unstakeItor != _unstaking.end(), "Restaking request not found" );

	if ( stakeItor == _staking.end() )
	{
		_staking.emplace( get_self(), [&]( auto &a ) {
			a.account = account;
			a.quantity = unstakeItor->quantity;
		} );
	}
	else
	{
		_staking.modify( stakeItor, get_self(), [&]( auto &a ) { a.quantity += unstakeItor->quantity; } );
	}

	_totalStake.quantity += unstakeItor->quantity;
	_unstaking.erase( unstakeItor );
}

void token::_refund( const name &account )
{
	auto _unstaking = unstaking_index( get_self(), get_self().value );
	auto unstakeItor = _unstaking.find( account.value );

	check( unstakeItor != _unstaking.end(), "Unstaking request not found" );
	check( unstakeItor->req_time.sec_since_epoch() + refund_delay_sec <= current_time_point().sec_since_epoch(),
		   "Refund is not available yet" );

	token::transfer_action( get_self(), { "felixstaking"_n, "active"_n } )
		.send( "felixstaking"_n, account, unstakeItor->quantity, "unstake" );
	_unstaking.erase( unstakeItor );
}

void token::refund( const name &account )
{
	require_auth( account );
	_refund( account );
}

void token::refunddef( const name &account )
{
	require_auth( get_self() );
	auto _unstaking = unstaking_index( get_self(), get_self().value );
	auto unstakeItor = _unstaking.find( account.value );

	check( unstakeItor != _unstaking.end(), "Unstaking request not found" );

	token::transfer_action( get_self(), { "felixstaking"_n, "active"_n } )
		.send( "felixstaking"_n, account, unstakeItor->quantity, "unstake" );
	_unstaking.erase( unstakeItor );
}

void token::dividend( const name &account, const asset &baseAmount, const asset &totalAmount )
{
	require_auth( get_self() );
	auto _staking = staking_index( get_self(), get_self().value );
	auto stakeItor = _staking.find( account.value );

	check( stakeItor != _staking.end(), "No staking account" );

	double ratio = static_cast<double>( stakeItor->quantity.amount ) / static_cast<double>( totalAmount.amount );
	// double ratio =
	// 	static_cast<double>( stakeItor->quantity.amount ) / static_cast<double>( _totalStake.quantity.amount );
	asset quantity = asset( static_cast<int64_t>( baseAmount.amount * ratio ), eos_symbol );
	// check( quantity.amount > 0, "Dividend amount is must greater than zero" );
	if ( quantity.amount > 0 )
	{
		action( permission_level{ "felixfunding"_n, "active"_n }, "eosio.token"_n, "transfer"_n,
				std::make_tuple( "felixfunding"_n, account, quantity, std::string( "dividend" ) ) )
			.send();
	}
}

void token::banish( const name &account )
{
	require_auth( get_self() );

	auto _staking = staking_index( get_self(), get_self().value );
	auto stakeItor = _staking.find( account.value );

	// check( stakeItor != _staking.end(), "No staking account" );
	if ( stakeItor != _staking.end() )
	{
		auto quantity = stakeItor->quantity;
		_totalStake.quantity -= quantity;
		_staking.erase( stakeItor );

		token::transfer_action( get_self(), { "felixstaking"_n, "active"_n } )
			.send( "felixstaking"_n, "felixtokenio", quantity, "banish" );
	}

	auto _unstaking = unstaking_index( get_self(), get_self().value );
	auto unstakeItor = _unstaking.find( account.value );

	if ( unstakeItor != _unstaking.end() )
	{
		auto quantity = unstakeItor->quantity;
		_totalStake.quantity += quantity;
		_unstaking.erase( unstakeItor );
	}
}

void token::recalc()
{
	require_auth( get_self() );

	auto _staking = staking_index( get_self(), get_self().value );

	asset totalAmount = asset( 0, felix_symbol );
	for ( auto itor = _staking.begin() ; itor != _staking.end() ; ++itor )
	{
		totalAmount.amount += itor->quantity.amount;
	}
	_totalStake.quantity = totalAmount;
}

void token::on_error( const eosio::onerror &error )
{
	transaction failed_tx = error.unpack_sent_trx();
	failed_tx.send( error.sender_id, get_self() );
	resend_action( get_self(), { get_self(), "active"_n } ).send( failed_tx, error.sender_id );
}

ACTION token::resend( eosio::transaction trx, uint128_t sender_id ) { require_auth( get_self() ); }

} // namespace felix
