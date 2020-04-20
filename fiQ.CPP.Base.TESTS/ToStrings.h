#pragma once

namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template<>
			std::wstring ToString<unsigned short>(const unsigned short& us) { return std::to_wstring(us); }
		}
	}
}
