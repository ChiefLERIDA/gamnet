#ifndef __GAMNET_LIB_DEBUGS_H_
#define __GAMNET_LIB_DEBUGS_H_

namespace Gamnet {

	// derived �� base ���� ��ӵ� Ŭ������ check_derived::assertion �� true
	template<typename derived, typename base> struct check_derived
	{
		static struct No {} Check(...);
		static struct Yes { int c; } Check(base*);

	public:
		enum { assertion = sizeof(Check(static_cast<derived*>(0))) == sizeof(Yes) };
	};

#define GAMNET_WHERE(derived, base) static_assert(Gamnet::check_derived<derived, base>::assertion, #derived " was not derived " #base " class")

}
#endif 

