//==========================================================================================================================
// LogSink.cpp : Classes and functions for application logging
//==========================================================================================================================
#include "pch.h"
#include "LogSink.h"
#include "ConsoleSink.h"
using namespace FIQCPPBASE;

LogSink& LogSink::GetSink() {
	static ConsoleSink ls;
	return ls;
}