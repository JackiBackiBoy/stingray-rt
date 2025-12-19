#pragma once

#include <type_traits>

template<typename E>
struct SREnableBitmaskOperators : std::false_type {};

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, E>::type operator|(E lhs, E rhs) {
	typedef typename std::underlying_type<E>::type underlying;
	return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, E&>::type operator|=(E& lhs, E rhs) {
	typedef typename std::underlying_type<E>::type underlying;
	lhs = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
	return lhs;
}

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, E>::type operator&(E lhs, E rhs) {
	typedef typename std::underlying_type<E>::type underlying;
	return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, E&>::type operator&=(E& lhs, E rhs) {
	typedef typename std::underlying_type<E>::type underlying;
	lhs = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
	return lhs;
}

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, E>::type operator~(E rhs) {
	typedef typename std::underlying_type<E>::type underlying;
	return static_cast<E>(~static_cast<underlying>(rhs));
}

template<typename E>
constexpr typename std::enable_if<SREnableBitmaskOperators<E>::value, bool>::type
has_flag(E lhs, E rhs) {
	return (lhs & rhs) == rhs;
}

#define SR_ENABLE_BITMASK_OPERATORS(EnumType)                                  \
    template<>                                                                 \
    struct SREnableBitmaskOperators<EnumType> : std::true_type {}