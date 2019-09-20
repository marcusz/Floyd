//
//  type_interner.hpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-29.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef type_interner_hpp
#define type_interner_hpp

#include "typeid.h"
#include "quark.h"

#include <variant>
#include <vector>
#include <map>

struct json_t;

namespace floyd {



//////////////////////////////////////////////////		itype_t

//	IMPORTANT: Collect all used types in a vector so we can use itype_t as an index into it for O(1)
/*
	bits 31 - 28 base_type (4 bits)
	bits 27 - 24 base_type 2 (4 bits) -- used to tell basetype of vector element or dictionary value.
	bits 23 - 0 intern_index (24 bits)
*/

struct itype_t {
	explicit itype_t(int32_t data) :
		data(data)
	{

#if DEBUG
	debug_bt0 = get_base_type();
	debug_lookup_index = get_lookup_index();
#endif


		QUARK_ASSERT(check_invariant());
	}

	static itype_t make_undefined(){
		return itype_t(assemble((int)base_type::k_undefined, base_type::k_undefined, base_type::k_undefined));
	}
	static itype_t make_any(){
		return itype_t(assemble((int)base_type::k_any, base_type::k_any, base_type::k_undefined));
	}
	static itype_t make_void(){
		return itype_t(assemble((int)base_type::k_void, base_type::k_void, base_type::k_undefined));
	}
	static itype_t make_bool(){
		return itype_t(assemble((int)base_type::k_bool, base_type::k_bool, base_type::k_undefined));
	}
	static itype_t make_int(){
		return itype_t(assemble((int)base_type::k_int, base_type::k_int, base_type::k_undefined));
	}
	static itype_t make_double(){
		return itype_t(assemble((int)base_type::k_double, base_type::k_double, base_type::k_undefined));
	}
	static itype_t make_string(){
		return itype_t(assemble((int)base_type::k_string, base_type::k_string, base_type::k_undefined));
	}
	static itype_t make_json(){
		return itype_t(assemble((int)base_type::k_json, base_type::k_json, base_type::k_undefined));
	}

	static itype_t make_typeid(){
		return itype_t(assemble((int)base_type::k_typeid, base_type::k_typeid, base_type::k_undefined));
	}

	static itype_t make_struct(uint32_t lookup_index){
		return itype_t(assemble(lookup_index, base_type::k_struct, base_type::k_undefined));
	}

	static itype_t make_vector(uint32_t lookup_index, base_type element_bt){
		return itype_t(assemble(lookup_index, base_type::k_vector, element_bt));
	}
	static itype_t make_dict(uint32_t lookup_index, base_type value_bt){
		return itype_t(assemble(lookup_index, base_type::k_dict, value_bt));
	}
	static itype_t make_function(uint32_t lookup_index){
		return itype_t(assemble(lookup_index, base_type::k_function, base_type::k_undefined));
	}

	static itype_t make_identifier(uint32_t lookup_index){
		return itype_t(assemble(lookup_index, base_type::k_identifier, base_type::k_undefined));
	}



	bool check_invariant() const {
		return true;
	}


	bool is_undefined() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_undefined;
	}


	bool is_any() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_any;
	}
	bool is_void() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_void;
	}
	bool is_bool() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_bool;
	}
	bool is_int() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_int;
	}
	bool is_double() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_double;
	}


	bool is_string() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_string;
	}
	bool is_json() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_json;
	}

	bool is_typeid() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_typeid;
	}



	bool is_struct() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_struct;
	}
	bool is_vector() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_vector;
	}
	bool is_dict() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_dict;
	}

	bool is_function() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_function;
	}

	bool is_identifier() const {
		QUARK_ASSERT(check_invariant());

		return get_base_type() == base_type::k_identifier;
	}




	base_type get_base_type() const {
		QUARK_ASSERT(check_invariant());

		const auto value = (data >> 28) & 15;
		const auto bt = static_cast<base_type>(value);
		return bt;
	}

	base_type get_vector_element_type() const {
		QUARK_ASSERT(check_invariant());
		QUARK_ASSERT(is_vector());

		const auto value = (data >> 24) & 15;
		const auto bt = static_cast<base_type>(value);
		return bt;
	}
	base_type get_dict_value_type() const {
		QUARK_ASSERT(check_invariant());
		QUARK_ASSERT(is_vector());

		const auto value = (data >> 24) & 15;
		const auto bt = static_cast<base_type>(value);
		return bt;
	}

	inline uint32_t get_lookup_index() const {
		QUARK_ASSERT(check_invariant());

		return data & 0b00000000'11111111'11111111'11111111;
	}
	int32_t get_data() const {
		QUARK_ASSERT(check_invariant());

		return data;
	}

	static uint32_t assemble(uint32_t lookup_index, base_type bt1, base_type bt2){
		const auto a = static_cast<uint32_t>(bt1);
		const auto b = static_cast<uint32_t>(bt2);

		return (a << 28) | (b << 24) | lookup_index;
	}

	static itype_t assemble2(uint32_t lookup_index, base_type bt1, base_type bt2){
		return itype_t(assemble(lookup_index, bt1, bt2));
	}

	////////////////////////////////	STATE

	private: uint32_t data;
#if DEBUG
	private: base_type debug_bt0;
	private: uint32_t debug_lookup_index;
#endif
};

inline bool operator<(itype_t lhs, itype_t rhs){
	return lhs.get_data() < rhs.get_data();
}
inline bool operator==(itype_t lhs, itype_t rhs){
	return lhs.get_data() == rhs.get_data();
}

inline itype_t get_undefined_itype(){
	return itype_t::make_undefined();
}
inline itype_t get_json_itype(){
	return itype_t::make_json();
}

inline bool is_empty(const itype_t& type){
	return type == itype_t::make_undefined();
}

inline bool is_atomic_type(itype_t type);


json_t itype_to_json(const itype_t& itype);
itype_t itype_from_json(const json_t& j);



//////////////////////////////////////////////////		type_interner_t



//	Assigns 32 bit ID to types. You can lookup the type using the ID.
//	This allows us to describe a type using a single 32 bit integer (compact, fast to copy around).
//	Each type has exactly ONE ID.
//	Automatically insert all basetype-types so they ALWAYS have EXPLICIT integer IDs as itypes.

struct type_interner_t {
	type_interner_t();

	bool check_invariant() const;


	////////////////////////////////	STATE

	//	All types are recorded here, an uniqued. Including tagged types.
	//	itype uses the INDEX into this array for fast lookups.
	std::vector<std::pair<type_tag_t, typeid_t>> interned2;
};


//	Records AND resolves the type. The returned type may be improved over input type.
itype_t intern_anonymous_type(type_interner_t& interner, const typeid_t& type);

//	Allocates a new itype for this tag. The tag must not already exist.
//	Interns the type for this tag. You can use typeid_t::make_undefined() and later update the type using update_tagged_type()
itype_t new_tagged_type(type_interner_t& interner, const type_tag_t& tag, const typeid_t& type);

//	Update the tagged type's type. The tagged type must already exist. Any usage of this tag will also get the new type.
void update_tagged_type(type_interner_t& interner, const type_tag_t& tag, const typeid_t& type);

itype_t lookup_itype_from_typeid(const type_interner_t& interner, const typeid_t& type);
inline const typeid_t& lookup_typeid_from_itype(const type_interner_t& interner, const itype_t& type);
inline const std::pair<type_tag_t, typeid_t>& lookup_typeinfo_from_itype(const type_interner_t& interner, const itype_t& type);
itype_t lookup_itype_from_tagged_type(const type_interner_t& interner, const type_tag_t& tag);


void trace_type_interner(const type_interner_t& interner);


//	Makes the type concrete by expanding any indirections via identifiers.
typeid_t explore_type_description(const type_interner_t& interner, const itype_t& type);

//	Compares the desci
bool compare_types_structurally(const type_interner_t& interner, const typeid_t& lhs, const typeid_t& rhs);



//////////////////////////////////////////////////		ast_type_t

/*
	This is how the C++ AST structs define a Floyd type.

	It can be
	1. A type description using optional_typeid, including undefined and unresolved_identifier.
	2. An itype into the type_interner.

	Notice that both 1 & 2 may contain subtypes that are undefined or uses unresolved_identifiers.

	ast_type_t is built ontop of the type_interner and itype.
*/

struct ast_type_t;
ast_type_t make_type_name_from_typeid(const typeid_t& t);
ast_type_t to_asttype(const itype_t& t);

struct ast_type_t {
	bool check_invariant() const {
		return true;
	}

	inline bool is_undefined() const ;

	static ast_type_t make_undefined() {
		return make_type_name_from_typeid(typeid_t::make_undefined());
	}

	static ast_type_t make_bool() {
		return make_type_name_from_typeid(typeid_t::make_bool());
	}
	static ast_type_t make_int() {
		return make_type_name_from_typeid(typeid_t::make_int());
	}
	static ast_type_t make_double() {
		return make_type_name_from_typeid(typeid_t::make_double());
	}
	static ast_type_t make_string() {
		return make_type_name_from_typeid(typeid_t::make_string());
	}
	static ast_type_t make_json() {
		return make_type_name_from_typeid(typeid_t::make_json());
	}


	////////////////////////////////////////		STATE

	typedef std::variant<
		std::monostate,
		typeid_t,
		itype_t
	> type_variant_t;

	type_variant_t _contents;
};

inline typeid_t get_typeid(const ast_type_t& type){
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(std::holds_alternative<typeid_t>(type._contents));

	return std::get<typeid_t>(type._contents);
}
inline itype_t get_itype(const ast_type_t& type){
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(std::holds_alternative<itype_t>(type._contents));

	return std::get<itype_t>(type._contents);
}

inline ast_type_t make_no_asttype(){
	return { std::monostate() };
}
inline bool is_empty(const ast_type_t& name){
	return std::holds_alternative<std::monostate>(name._contents);
}

inline bool operator==(const ast_type_t& lhs, const ast_type_t& rhs){
	return lhs._contents == rhs._contents;
}

inline bool ast_type_t::is_undefined() const {
	return (*this) == make_undefined();
}

json_t ast_type_to_json(const ast_type_t& name);
ast_type_t ast_type_from_json(const json_t& j);

std::string ast_type_to_string(const ast_type_t& type);


itype_t intern_anonymous_type(type_interner_t& interner, const ast_type_t& type);

//	Returns typeid_t::make_undefined() if ast_type_t is in monostate mode
itype_t lookup_itype_from_asttype(const type_interner_t& interner, const ast_type_t& type);





//////////////////////////////////////////////////		INLINES



//	Used at runtime.
inline const typeid_t& lookup_typeid_from_itype(const type_interner_t& interner, const itype_t& type){
	QUARK_ASSERT(interner.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < interner.interned2.size());

	const auto& result = interner.interned2[lookup_index].second;
	return result;
}

inline const std::pair<type_tag_t, typeid_t>& lookup_typeinfo_from_itype(const type_interner_t& interner, const itype_t& type){
	QUARK_ASSERT(interner.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < interner.interned2.size());

	const auto& result = interner.interned2[lookup_index];
	return result;
}


inline bool is_atomic_type(itype_t type){
	const auto bt = type.get_base_type();
	if(
		bt == base_type::k_undefined
		|| bt == base_type::k_any
		|| bt == base_type::k_void

		|| bt == base_type::k_bool
		|| bt == base_type::k_int
		|| bt == base_type::k_double
		|| bt == base_type::k_string
		|| bt == base_type::k_json

		|| bt == base_type::k_typeid
		|| bt == base_type::k_identifier
	){
		return true;
	}
	else{
		return false;
	}
}



/*
Do this without using typeid_t at all.
inline itype_t get_vector_element_type_quick(const type_interner_t& interner, itype_t vec){
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(vec.is_vector());

	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < interner.interned.size());

	const auto& result = interner.interned[lookup_index];
	return result;


	const auto value = (data >> 24) & 15;
	const auto bt = static_cast<base_type>(value);
	return bt;
}

inline itype_t get_dict_value_type(const type_interner_t& interner, itype_t vec) const {
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(vec.is_vector());

	const auto value = (data >> 24) & 15;
	const auto bt = static_cast<base_type>(value);
	return bt;
}
*/



}	//	floyd

#endif /* type_interner_hpp */
