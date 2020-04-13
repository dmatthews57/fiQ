// fiQ.CPP.Base.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "Tools/ValueOps.h"

using namespace FIQCPPBASE;

// TODO: This is an example of a library function
void fnfiQCPPBase()
{
	ValueOps::Is(5).InSet(1, 2, 3, 4, 6);
}
