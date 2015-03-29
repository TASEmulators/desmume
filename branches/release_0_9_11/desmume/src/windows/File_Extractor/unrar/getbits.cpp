#include "rar.hpp"

BitInput::BitInput()
{
	InBuf = (byte*) rarmalloc( MAX_SIZE );
	
	// Otherwise getbits() reads uninitialized memory
	// TODO: instead of clearing entire block, just clear last two
	// bytes after reading from file
	if ( InBuf )
		memset( InBuf, 0, MAX_SIZE );
}

BitInput::~BitInput()
{
	rarfree( InBuf );
}

void BitInput::handle_mem_error( Rar_Error_Handler& ErrHandler )
{
	if ( !InBuf )
		ErrHandler.MemoryError();
}

void BitInput::faddbits(int Bits)
{
	addbits(Bits);
}


unsigned int BitInput::fgetbits()
{
	return(getbits());
}
