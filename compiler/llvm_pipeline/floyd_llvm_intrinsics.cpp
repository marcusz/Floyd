//
//  floyd_llvm_intrinsics.cpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-28.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "floyd_llvm_intrinsics.h"


#include "floyd_llvm_helpers.h"
#include "floyd_llvm_runtime.h"
#include "floyd_llvm_codegen_basics.h"
#include "value_features.h"
#include "floyd_runtime.h"
#include "text_parser.h"

#include "utils.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace floyd {


static const bool k_trace_function_link_map = false;




static std::string gen_to_string(llvm_execution_engine_t& runtime, runtime_value_t arg_value, runtime_type_t arg_type){
	QUARK_ASSERT(runtime.check_invariant());

	const auto& types = runtime.backend.types;
	const auto& type = lookup_type_ref(runtime.backend, arg_type);

	if(peek2(types, type).is_typeid()){
		const auto value = from_runtime_value(runtime, arg_value, type);
		const auto type2 = value.get_typeid_value();
		const auto type3 = peek2(types, type2);
		const auto a = type_to_compact_string(types, type3);
		return a;
	}
	else{
		const auto value = from_runtime_value(runtime, arg_value, type);
		const auto a = to_compact_string2(runtime.backend.types, value);
		return a;
	}
}

static llvm::FunctionType* make_intrinsic_llvm_function_type(const llvm_type_lookup& type_lookup, const intrinsic_signature_t& signature){
	return (llvm::FunctionType*)deref_ptr(get_llvm_type_as_arg(type_lookup, signature._function_type));
}



/////////////////////////////////////////		specialization_t


/*
POD VS NONPOD

								return		arg0		arg1		arg2
	------------------------------------------------------------------------------------------------
	update()					any			any			any			any

	update_string()				string		string		int			int
	update_vector_carray()		vec<T>		vec<T>		int			T
	update_vector_hamt()		vec<T>		vec<T>		int			T

	update_dict_cppmap()		dict<T>		dict<T>		string		T
	update_dict_hamt()			dict<T>		dict<T>		string		T
*/

enum class eresolved_type {
	k_string,

	k_vector_carray_pod,
	k_vector_carray_nonpod,
	k_vector_hamt_pod,
	k_vector_hamt_nonpod,

	k_dict_cppmap_pod,
	k_dict_cppmap_nonpod,

	k_dict_hamt_pod,
	k_dict_hamt_nonpod,

	k_json
};

struct specialization_t {
	eresolved_type required_arg_type;
	function_bind_t bind;
};

static bool matches_specialization(const config_t& config, const types_t& types, eresolved_type wanted, const type_t& arg_type){
	QUARK_ASSERT(config.check_invariant());
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(arg_type.check_invariant());

	const auto arg_type_peek = peek2(types, arg_type);
	if(arg_type_peek.is_string()){
		return wanted == eresolved_type::k_string;
	}
	else if(is_vector_carray(types, config, arg_type)){
		const auto is_rc = is_rc_value(peek2(types, arg_type_peek.get_vector_element_type(types)));
		if(is_rc){
			return wanted == eresolved_type::k_vector_carray_nonpod;
		}
		else{
			return wanted == eresolved_type::k_vector_carray_pod;
		}
	}
	else if(is_vector_hamt(types, config, arg_type)){
		const auto is_rc = is_rc_value(peek2(types, arg_type_peek.get_vector_element_type(types)));
		if(is_rc){
			return wanted == eresolved_type::k_vector_hamt_nonpod;
		}
		else{
			return wanted == eresolved_type::k_vector_hamt_pod;
		}
	}

	else if(is_dict_cppmap(types, config, arg_type)){
		const auto is_rc = is_rc_value(peek2(types, arg_type_peek.get_dict_value_type(types)));
		if(is_rc){
			return wanted == eresolved_type::k_dict_cppmap_nonpod;
		}
		else{
			return wanted == eresolved_type::k_dict_cppmap_pod;
		}
	}
	else if(is_dict_hamt(types, config, arg_type)){
		const auto is_rc = is_rc_value(peek2(types, arg_type_peek.get_dict_value_type(types)));
		if(is_rc){
			return wanted == eresolved_type::k_dict_hamt_nonpod;
		}
		else{
			return wanted == eresolved_type::k_dict_hamt_pod;
		}
	}

	else if(arg_type_peek.is_json()){
		return wanted == eresolved_type::k_json;
	}

	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}




static const function_link_entry_t& lookup_link_map(const config_t& config, const types_t& types, const std::vector<function_link_entry_t>& link_map, const std::vector<specialization_t>& specialisations, const type_t& type){
	QUARK_ASSERT(type.check_invariant());

	const auto it = std::find_if(specialisations.begin(), specialisations.end(), [&](const specialization_t& s) { return matches_specialization(config, types, s.required_arg_type, type); });
	if(it == specialisations.end()){
		QUARK_ASSERT(false);
		throw std::exception();
	}
	const auto& res = find_function_def_from_link_name(link_map, encode_intrinsic_link_name(it->bind.name));
	return res;
}


/////////////////////////////////////////		assert()



static void floyd_llvm_intrinsic__assert(floyd_runtime_t* frp, runtime_value_t arg){
	auto& r = get_floyd_runtime(frp);

//	QUARK_ASSERT(arg.bool_value == 0 || arg.bool_value == 1);

	bool ok = (arg.bool_value & 0x01) == 0 ? false : true;
	if(!ok){
		r._print_output.push_back("Assertion failed.");
		quark::throw_runtime_error("Floyd assertion failed.");
	}
}



/////////////////////////////////////////		erase()



//??? optimize prio 1
//??? all types are compile-time only.
static runtime_value_t floyd_llvm_intrinsic__erase(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type){
	auto& r = get_floyd_runtime(frp);

	const auto& types = r.backend.types;
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);

	QUARK_ASSERT(peek2(types, type0).is_dict());
	QUARK_ASSERT(peek2(types, type1).is_string());

	if(is_dict_cppmap(types, r.backend.config, type0)){
		const auto& dict = unpack_dict_cppmap_arg(r.backend, coll_value, coll_type);

		const auto value_type = peek2(types, type0).get_dict_value_type(types);

		//	Deep copy dict.
		auto dict2 = alloc_dict_cppmap(r.backend.heap, type0);
		auto& m = dict2.dict_cppmap_ptr->get_map_mut();
		m = dict->get_map();

		const auto key_string = from_runtime_string(r, key_value);
		m.erase(key_string);

		if(is_rc_value(peek2(types, value_type))){
			for(auto& e: m){
				retain_value(r.backend, e.second, value_type);
			}
		}
		return dict2;
	}
	else if(is_dict_hamt(types, r.backend.config, type0)){
		const auto& dict = *coll_value.dict_hamt_ptr;

		const auto value_type = peek2(types, type0).get_dict_value_type(types);

		//	Deep copy dict.
		auto dict2 = alloc_dict_hamt(r.backend.heap, type0);
		auto& m = dict2.dict_hamt_ptr->get_map_mut();
		m = dict.get_map();

		const auto key_string = from_runtime_string(r, key_value);
		m = m.erase(key_string);

		if(is_rc_value(peek2(types, value_type))){
			for(auto& e: m){
				retain_value(r.backend, e.second, value_type);
			}
		}
		return dict2;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		get_keys()



//??? optimize prio 1
//??? We need to figure out the return type *again*, knowledge we have already in semast.
static runtime_value_t floyd_llvm_intrinsic__get_keys(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type){
	auto& r = get_floyd_runtime(frp);

	const auto& types = r.backend.types;
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	QUARK_ASSERT(peek2(types, type0).is_dict());

	if(is_dict_cppmap(types, r.backend.config, type0)){
		if(r.backend.config.vector_backend_mode == vector_backend::carray){
			return get_keys__cppmap_carray(r.backend, coll_value, coll_type);
		}
		else if(r.backend.config.vector_backend_mode == vector_backend::hamt){
			return get_keys__cppmap_hamt(r.backend, coll_value, coll_type);
		}
		else{
			QUARK_ASSERT(false);
			throw std::exception();
		}
	}
	else if(is_dict_hamt(types, r.backend.config, type0)){
		if(r.backend.config.vector_backend_mode == vector_backend::carray){
			return get_keys__hamtmap_carray(r.backend, coll_value, coll_type);
		}
		else if(r.backend.config.vector_backend_mode == vector_backend::hamt){
			return get_keys__hamtmap_hamt(r.backend, coll_value, coll_type);
		}
		else{
			QUARK_ASSERT(false);
			throw std::exception();
		}
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		exists()



//??? optimize prio 1
//??? kill unpack_dict_cppmap_arg()
static uint32_t floyd_llvm_intrinsic__exists(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto& types = r.backend.types;
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, value_type);
	QUARK_ASSERT(peek2(types, type0).is_dict());

	if(is_dict_cppmap(types, r.backend.config, type0)){
		const auto& dict = unpack_dict_cppmap_arg(r.backend, coll_value, coll_type);
		const auto key_string = from_runtime_string(r, value);

		const auto& m = dict->get_map();
		const auto it = m.find(key_string);
		return it != m.end() ? 1 : 0;
	}
	else if(is_dict_hamt(types, r.backend.config, type0)){
		const auto& dict = *coll_value.dict_hamt_ptr;
		const auto key_string = from_runtime_string(r, value);

		const auto& m = dict.get_map();
		const auto it = m.find(key_string);
		return it != nullptr ? 1 : 0;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		find()



//??? optimize prio 1
static int64_t floyd_llvm_intrinsic__find(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, const runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	if(peek2(r.backend.types, type0).is_string()){
		return find__string(r.backend, coll_value, coll_type, value, value_type);
	}
	else if(is_vector_carray(r.backend.types, r.backend.config, type0)){
		return find__carray(r.backend, coll_value, coll_type, value, value_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type0)){
		return find__hamt(r.backend, coll_value, coll_type, value, value_type);
	}
	else{
		//	No other types allowed.
		UNSUPPORTED();
	}
}



/////////////////////////////////////////		get_json_type()



static int64_t floyd_llvm_intrinsic__get_json_type(floyd_runtime_t* frp, JSON_T* json_ptr){
	auto& r = get_floyd_runtime(frp);
	(void)r;
	QUARK_ASSERT(json_ptr != nullptr);

	const auto& json = json_ptr->get_json();
	const auto result = get_json_type(json);
	return result;
}



/////////////////////////////////////////		generate_json_script()



static runtime_value_t floyd_llvm_intrinsic__generate_json_script(floyd_runtime_t* frp, JSON_T* json_ptr){
	auto& r = get_floyd_runtime(frp);
	QUARK_ASSERT(json_ptr != nullptr);

	const auto& json = json_ptr->get_json();

	const std::string s = json_to_compact_string(json);
	return to_runtime_string(r, s);
}



/////////////////////////////////////////		from_json()



static runtime_value_t floyd_llvm_intrinsic__from_json(floyd_runtime_t* frp, JSON_T* json_ptr, runtime_type_t target_type){
	auto& r = get_floyd_runtime(frp);
	QUARK_ASSERT(json_ptr != nullptr);

	const auto& json = json_ptr->get_json();
	const auto& target_type2 = lookup_type_ref(r.backend, target_type);

	const auto result = unflatten_json_to_specific_type(r.backend.types, json, target_type2);
	const auto result2 = to_runtime_value(r, result);
	return result2;
}




/////////////////////////////////////////		map()




//??? Use C++ template to generate these two functions.
typedef runtime_value_t (*MAP_F)(floyd_runtime_t* frp, runtime_value_t e_value, runtime_value_t context_value);

static runtime_value_t map__carray(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t f_value, runtime_type_t f_type, runtime_value_t context_value, runtime_type_t context_type, runtime_type_t result_vec_type){
	auto& r = get_floyd_runtime(frp);
	auto& backend = r.backend;
	const auto& types = r.backend.types;

	QUARK_ASSERT(backend.check_invariant());

#if DEBUG
	const auto& type1 = lookup_type_ref(backend, f_type);

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type2 = lookup_type_ref(backend, context_type);
//	QUARK_ASSERT(check_map_func_type(type0, type1, type2));

	const auto e_type = peek2(types, type0).get_vector_element_type(types);
	const auto f_arg_types = peek2(types, type1).get_function_args(types);
#endif

	const auto f = reinterpret_cast<MAP_F>(f_value.function_ptr);

	const auto count = elements_vec.vector_carray_ptr->get_element_count();
	auto result_vec = alloc_vector_carray(backend.heap, count, count, type_t(result_vec_type));
	for(int i = 0 ; i < count ; i++){
		const auto a = (*f)(frp, elements_vec.vector_carray_ptr->get_element_ptr()[i], context_value);
		result_vec.vector_carray_ptr->get_element_ptr()[i] = a;
	}
	return result_vec;
}
//??? Update 1 element in a big hamt will copy the entire hamt, inc RC on all elements in hamt2. This is not needed since most of hamt is shared. Cheaper if we build in RC for leaf in the hamt itself.
//??? Use batching to speed up hamt creation. Add 32 nodes at a time. Also faster read iteration.
static runtime_value_t map__hamt(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t f_value, runtime_type_t f_type, runtime_value_t context_value, runtime_type_t context_type, runtime_type_t result_vec_type){
	auto& r = get_floyd_runtime(frp);
	auto& backend = r.backend;
	QUARK_ASSERT(backend.check_invariant());
	const auto& types = r.backend.types;

#if DEBUG
	const auto& type1 = lookup_type_ref(backend, f_type);

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type2 = lookup_type_ref(backend, context_type);
//	QUARK_ASSERT(check_map_func_type(type0, type1, type2));

	const auto e_type = peek2(types, type0).get_vector_element_type(types);
	const auto f_arg_types = peek2(types, type1).get_function_args(types);
#endif

	const auto f = reinterpret_cast<MAP_F>(f_value.function_ptr);

	const auto count = elements_vec.vector_hamt_ptr->get_element_count();
	auto result_vec = alloc_vector_hamt(backend.heap, count, count, type_t(result_vec_type));
	for(int i = 0 ; i < count ; i++){
		const auto& element = elements_vec.vector_hamt_ptr->load_element(i);
		const auto a = (*f)(frp, element, context_value);
		result_vec.vector_hamt_ptr->store_mutate(i, a);
	}
	return result_vec;
}

//	[R] map([E] elements, func R (E e, C context) f, C context)
static std::vector<specialization_t> make_map_specializations(llvm::LLVMContext& context, const llvm_type_lookup& type_lookup){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
		{
			make_frp_type(type_lookup),

			make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup),

			make_runtime_value_type(type_lookup),
			make_runtime_type_type(type_lookup),

			make_runtime_value_type(type_lookup),
			make_runtime_type_type(type_lookup),

			make_runtime_type_type(type_lookup)
		},
		false
	);
	return {
		specialization_t { eresolved_type::k_vector_carray_pod,		{ "map_carray_pod", function_type, reinterpret_cast<void*>(map__carray) } },
		specialization_t { eresolved_type::k_vector_carray_nonpod,	{ "map_carray_nonpod", function_type, reinterpret_cast<void*>(map__carray) } },
		specialization_t { eresolved_type::k_vector_hamt_pod,		{ "map_hamt_pod", function_type, reinterpret_cast<void*>(map__hamt) } },
		specialization_t { eresolved_type::k_vector_hamt_nonpod, 	{ "map_hamt_nonpod", function_type, reinterpret_cast<void*>(map__hamt) } }
	};
}

llvm::Value* generate_instrinsic_map(
	llvm_function_generator_t& gen_acc,
	const type_t& resolved_call_type,
	llvm::Value& elements_vec_reg,
	const type_t& elements_vec_type,
	llvm::Value& f_reg,
	const type_t& f_type,
	llvm::Value& context_reg,
	const type_t& context_type)
{
	QUARK_ASSERT(gen_acc.check_invariant());
	QUARK_ASSERT(elements_vec_type.check_invariant());

	const auto& types = gen_acc.gen.type_lookup.state.types;
	auto& builder = gen_acc.get_builder();
	const auto res = lookup_link_map(gen_acc.gen.settings.config, types, gen_acc.gen.link_map, make_map_specializations(builder.getContext(), gen_acc.gen.type_lookup), elements_vec_type);

	const auto result_vec_type = peek2(types, resolved_call_type).get_function_return(types);
	return builder.CreateCall(
		res.llvm_codegen_f,
		{
			gen_acc.get_callers_fcp(),

			&elements_vec_reg,
			generate_itype_constant(gen_acc.gen, elements_vec_type),

			generate_cast_to_runtime_value(gen_acc.gen, f_reg, f_type),
			generate_itype_constant(gen_acc.gen, f_type),

			generate_cast_to_runtime_value(gen_acc.gen, context_reg, context_type),
			generate_itype_constant(gen_acc.gen, context_type),

			generate_itype_constant(gen_acc.gen, result_vec_type)
		},
		""
	);
}





/////////////////////////////////////////		map_string()




//??? Can mutate the acc string internally.

typedef runtime_value_t (*MAP_STRING_F)(floyd_runtime_t* frp, runtime_value_t s, runtime_value_t context);

static runtime_value_t floyd_llvm_intrinsic__map_string(floyd_runtime_t* frp, runtime_value_t input_string0, runtime_value_t func, runtime_type_t func_type, runtime_value_t context, runtime_type_t context_type){
	auto& r = get_floyd_runtime(frp);

/*
	QUARK_ASSERT(check_map_string_func_type(
		type_t::make_string(),
		lookup_type_ref(r.backend, func_type),
		lookup_type_ref(r.backend, context_type)
	));
*/

	const auto f = reinterpret_cast<MAP_STRING_F>(func.function_ptr);

	const auto input_string = from_runtime_string(r, input_string0);

	auto count = input_string.size();

	std::string acc;
	for(int i = 0 ; i < count ; i++){
		const int64_t ch = input_string[i];
		const auto ch2 = make_runtime_int(ch);
		const auto temp = (*f)(frp, ch2, context);

		const auto temp2 = temp.int_value;
		acc.push_back(static_cast<char>(temp2));
	}
	return to_runtime_string(r, acc);
}



/////////////////////////////////////////		map_dag()



// [R] map_dag([E] elements, [int] depends_on, func R (E, [R], C context) f, C context)

typedef runtime_value_t (*map_dag_F)(floyd_runtime_t* frp, runtime_value_t r_value, runtime_value_t r_vec_value, runtime_value_t context_value);

//??? optimize prio 2
static runtime_value_t map_dag__carray(
	floyd_runtime_t* frp,
	value_backend_t& backend,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t depends_on_vec,
	runtime_type_t depends_on_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context,
	runtime_type_t context_type
){
	QUARK_ASSERT(frp != nullptr);
	QUARK_ASSERT(backend.check_invariant());
	const auto& types = backend.types;
	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, depends_on_vec_type);
	const auto& type2 = lookup_type_ref(backend, f_value_type);
//	QUARK_ASSERT(check_map_dag_func_type(type0, type1, type2, lookup_type_ref(backend, context_type)));

	const auto& elements = elements_vec;
#if DEBUG
	const auto& e_type = peek2(types, type0).get_vector_element_type(types);
#endif
	const auto& parents = depends_on_vec;
	const auto& f = f_value;
	const auto& r_type = peek2(types, type2).get_function_return(types);

	QUARK_ASSERT(e_type == peek2(types, type2).get_function_args(types)[0] && r_type == peek2(types, peek2(types, type2).get_function_args(types)[1]).get_vector_element_type(types));

//	QUARK_ASSERT(is_vector_carray(make_vector(e_type)));
//	QUARK_ASSERT(is_vector_carray(make_vector(r_type)));

	const auto return_type = make_vector(types, r_type);

	const auto f2 = reinterpret_cast<map_dag_F>(f.function_ptr);

	const auto elements2 = elements.vector_carray_ptr;
	const auto parents2 = parents.vector_carray_ptr;

	if(elements2->get_element_count() != parents2->get_element_count()) {
		quark::throw_runtime_error("map_dag() requires elements and parents be the same count.");
	}

	auto elements_todo = elements2->get_element_count();
	std::vector<int> rcs(elements2->get_element_count(), 0);

	std::vector<runtime_value_t> complete(elements2->get_element_count(), runtime_value_t());

	for(int i = 0 ; i < parents2->get_element_count() ; i++){
		const auto& e = parents2->load_element(i);
		const auto parent_index = e.int_value;

		const auto count = static_cast<int64_t>(elements2->get_element_count());
		QUARK_ASSERT(parent_index >= -1);
		QUARK_ASSERT(parent_index < count);

		if(parent_index != -1){
			rcs[parent_index]++;
		}
	}

	while(elements_todo > 0){
		//	Find all elements that are free to process -- they are not blocked on a depenency.
		std::vector<int> pass_ids;
		for(int i = 0 ; i < elements2->get_element_count() ; i++){
			const auto rc = rcs[i];
			if(rc == 0){
				pass_ids.push_back(i);
				rcs[i] = -1;
			}
		}
		if(pass_ids.empty()){
			quark::throw_runtime_error("map_dag() dependency cycle error.");
		}

		for(const auto element_index: pass_ids){
			const auto& e = elements2->load_element(element_index);

			//	Make list of the element's inputs -- they must all be complete now.
			std::vector<runtime_value_t> solved_deps;
			for(int element_index2 = 0 ; element_index2 < parents2->get_element_count() ; element_index2++){
				const auto& p = parents2->load_element(element_index2);
				const auto parent_index = p.int_value;
				if(parent_index == element_index){
					QUARK_ASSERT(element_index2 != -1);
					QUARK_ASSERT(element_index2 >= -1 && element_index2 < elements2->get_element_count());
					QUARK_ASSERT(rcs[element_index2] == -1);

					const auto& solved = complete[element_index2];
					solved_deps.push_back(solved);
				}
			}

			auto solved_deps2 = alloc_vector_carray(backend.heap, solved_deps.size(), solved_deps.size(), return_type);
			for(int i = 0 ; i < solved_deps.size() ; i++){
				solved_deps2.vector_carray_ptr->store(i, solved_deps[i]);
			}
			runtime_value_t solved_deps3 = solved_deps2;

			const auto result1 = (*f2)(frp, e, solved_deps3, context);

			//	Release just the vec, **not the elements**. The elements are aliases for complete-vector.
			if(dec_rc(solved_deps2.vector_carray_ptr->alloc) == 0){
				dispose_vector_carray(solved_deps2);
			}

			const auto parent_index = parents2->load_element(element_index).int_value;
			if(parent_index != -1){
				rcs[parent_index]--;
			}
			complete[element_index] = result1;
			elements_todo--;
		}
	}

	//??? No need to copy all elements -- could store them directly into the VEC_T.
	const auto count = complete.size();
	auto result_vec = alloc_vector_carray(backend.heap, count, count, return_type);
	for(int i = 0 ; i < count ; i++){
//		retain_value(r, complete[i], r_type);
		result_vec.vector_carray_ptr->store(i, complete[i]);
	}

	return result_vec;
}

//??? optimize prio 2
static runtime_value_t map_dag__hamt(
	floyd_runtime_t* frp,
	value_backend_t& backend,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t depends_on_vec,
	runtime_type_t depends_on_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context,
	runtime_type_t context_type
){
	QUARK_ASSERT(frp != nullptr);
	QUARK_ASSERT(backend.check_invariant());

	const auto& types = backend.types;
	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, depends_on_vec_type);
	const auto& type2 = lookup_type_ref(backend, f_value_type);
//	QUARK_ASSERT(check_map_dag_func_type(type0, type1, type2, lookup_type_ref(backend, context_type)));

	const auto& elements = elements_vec;
#if DEBUG
	const auto& e_type = peek2(types, type0).get_vector_element_type(types);
#endif
	const auto& parents = depends_on_vec;
	const auto& f = f_value;
	const auto& r_type = peek2(types, type2).get_function_return(types);

	QUARK_ASSERT(e_type == peek2(types, type2).get_function_args(types)[0] && r_type == peek2(types, peek2(types, type2).get_function_args(types)[1]).get_vector_element_type(types));

//	QUARK_ASSERT(is_vector_hamt(make_vector(e_type)));
//	QUARK_ASSERT(is_vector_hamt(make_vector(r_type)));

	const auto return_type = make_vector(types, r_type);

	const auto f2 = reinterpret_cast<map_dag_F>(f.function_ptr);

	const auto elements2 = elements.vector_hamt_ptr;
	const auto parents2 = parents.vector_hamt_ptr;

	if(elements2->get_element_count() != parents2->get_element_count()) {
		quark::throw_runtime_error("map_dag() requires elements and parents be the same count.");
	}

	auto elements_todo = elements2->get_element_count();
	std::vector<int> rcs(elements2->get_element_count(), 0);

	std::vector<runtime_value_t> complete(elements2->get_element_count(), runtime_value_t());

	for(int i = 0 ; i < parents2->get_element_count() ; i++){
		const auto& e = parents2->load_element(i);
		const auto parent_index = e.int_value;

		//??? remove cast? static_cast<int64_t>()
		const auto count = static_cast<int64_t>(elements2->get_element_count());
		QUARK_ASSERT(parent_index >= -1);
		QUARK_ASSERT(parent_index < count);

		if(parent_index != -1){
			rcs[parent_index]++;
		}
	}

	while(elements_todo > 0){
		//	Find all elements that are free to process -- they are not blocked on a depenency.
		std::vector<int> pass_ids;
		for(int i = 0 ; i < elements2->get_element_count() ; i++){
			const auto rc = rcs[i];
			if(rc == 0){
				pass_ids.push_back(i);
				rcs[i] = -1;
			}
		}
		if(pass_ids.empty()){
			quark::throw_runtime_error("map_dag() dependency cycle error.");
		}

		for(const auto element_index: pass_ids){
			const auto& e = elements2->load_element(element_index);

			//	Make list of the element's inputs -- they must all be complete now.
			std::vector<runtime_value_t> solved_deps;
			for(int element_index2 = 0 ; element_index2 < parents2->get_element_count() ; element_index2++){
				const auto& p = parents2->load_element(element_index2);
				const auto parent_index = p.int_value;
				if(parent_index == element_index){
					QUARK_ASSERT(element_index2 != -1);
					QUARK_ASSERT(element_index2 >= -1 && element_index2 < elements2->get_element_count());
					QUARK_ASSERT(rcs[element_index2] == -1);

					const auto& solved = complete[element_index2];
					solved_deps.push_back(solved);
				}
			}

			auto solved_deps2 = alloc_vector_hamt(backend.heap, solved_deps.size(), solved_deps.size(), return_type);
			for(int i = 0 ; i < solved_deps.size() ; i++){
				solved_deps2.vector_hamt_ptr->store_mutate(i, solved_deps[i]);
			}
			runtime_value_t solved_deps3 = solved_deps2;

			const auto result1 = (*f2)(frp, e, solved_deps3, context);

			//	Release just the vec, **not the elements**. The elements are aliases for complete-vector.
			if(dec_rc(solved_deps2.vector_hamt_ptr->alloc) == 0){
				dispose_vector_hamt(solved_deps2);
			}

			const auto parent_index = parents2->load_element(element_index).int_value;
			if(parent_index != -1){
				rcs[parent_index]--;
			}
			complete[element_index] = result1;
			elements_todo--;
		}
	}

	//??? No need to copy all elements -- could store them directly into the VEC_T.
	const auto count = complete.size();
	auto result_vec = alloc_vector_hamt(backend.heap, count, count, return_type);
	for(int i = 0 ; i < count ; i++){
//		retain_value(r, complete[i], r_type);
		result_vec.vector_hamt_ptr->store_mutate(i, complete[i]);
	}

	return result_vec;
}

// ??? optimize prio 1: check type at compile time, not runtime.
// [R] map_dag([E] elements, [int] depends_on, func R (E, [R], C context) f, C context)
static runtime_value_t floyd_llvm_intrinsic__map_dag(
	floyd_runtime_t* frp,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t depends_on_vec,
	runtime_type_t depends_on_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context,
	runtime_type_t context_type
){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return map_dag__carray(frp, r.backend, elements_vec, elements_vec_type, depends_on_vec, depends_on_vec_type, f_value, f_value_type, context, context_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return map_dag__hamt(frp, r.backend, elements_vec, elements_vec_type, depends_on_vec, depends_on_vec_type, f_value, f_value_type, context, context_type);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		filter()



typedef runtime_value_t (*FILTER_F)(floyd_runtime_t* frp, runtime_value_t element_value, runtime_value_t context);

//??? optimize prio 1
static runtime_value_t filter__carray(floyd_runtime_t* frp, value_backend_t& backend, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t f_value, runtime_type_t f_value_type, runtime_value_t context, runtime_type_t context_type){
	QUARK_ASSERT(backend.check_invariant());

	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(r.backend, f_value_type);
	const auto& type2 = lookup_type_ref(r.backend, context_type);
	const auto& return_type = type0;

//	QUARK_ASSERT(check_filter_func_type(type0, type1, type2));
	QUARK_ASSERT(is_vector_carray(backend.types, r.backend.config, type_t(elements_vec_type)));

	const auto& vec = *elements_vec.vector_carray_ptr;
	const auto f = reinterpret_cast<FILTER_F>(f_value.function_ptr);

	auto count = vec.get_element_count();

	const auto e_element_itype = lookup_vector_element_type(backend, type_t(elements_vec_type));

	std::vector<runtime_value_t> acc;
	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.get_element_ptr()[i];
		const auto keep = (*f)(frp, element_value, context);
		if(keep.bool_value != 0){
			acc.push_back(element_value);

			if(is_rc_value(peek2(r.backend.types, e_element_itype))){
				retain_value(r.backend, element_value, e_element_itype);
			}
		}
		else{
		}
	}

	const auto count2 = acc.size();
	auto result_vec = alloc_vector_carray(r.backend.heap, count2, count2, return_type);

	if(count2 > 0){
		//	Count > 0 required to get address to first element in acc.
		copy_elements(result_vec.vector_carray_ptr->get_element_ptr(), &acc[0], count2);
	}
	return result_vec;
}

//??? optimize prio 1
static runtime_value_t filter__hamt(floyd_runtime_t* frp, value_backend_t& backend, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t f_value, runtime_type_t f_value_type, runtime_value_t context, runtime_type_t context_type){
	QUARK_ASSERT(backend.check_invariant());

	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(r.backend, f_value_type);
	const auto& type2 = lookup_type_ref(r.backend, context_type);
	const auto& return_type = type0;

//	QUARK_ASSERT(check_filter_func_type(type0, type1, type2));
	QUARK_ASSERT(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type)));

	const auto& vec = *elements_vec.vector_hamt_ptr;
	const auto f = reinterpret_cast<FILTER_F>(f_value.function_ptr);

	auto count = vec.get_element_count();

	const auto e_element_itype = lookup_vector_element_type(backend, type_t(elements_vec_type));

	std::vector<runtime_value_t> acc;
	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.load_element(i);
		const auto keep = (*f)(frp, element_value, context);
		if(keep.bool_value != 0){
			acc.push_back(element_value);

			if(is_rc_value(peek2(r.backend.types, e_element_itype))){
				retain_value(r.backend, element_value, e_element_itype);
			}
		}
		else{
		}
	}

	const auto count2 = acc.size();
	auto result_vec = alloc_vector_hamt(r.backend.heap, &acc[0], count2, return_type);
	return result_vec;
}

//??? optimize prio 1: check type at compile time, not runtime.
//	[E] filter([E] elements, func bool (E e, C context) f, C context)
static runtime_value_t floyd_llvm_intrinsic__filter(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t f_value, runtime_type_t f_value_type, runtime_value_t arg2_value, runtime_type_t arg2_type){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return filter__carray(frp, r.backend, elements_vec, elements_vec_type, f_value, f_value_type, arg2_value, arg2_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return filter__hamt(frp, r.backend, elements_vec, elements_vec_type, f_value, f_value_type, arg2_value, arg2_type);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		reduce()



typedef runtime_value_t (*REDUCE_F)(floyd_runtime_t* frp, runtime_value_t acc_value, runtime_value_t element_value, runtime_value_t context);

//??? optimize prio 1
static runtime_value_t reduce__carray(floyd_runtime_t* frp, value_backend_t& backend, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t init_value, runtime_type_t init_value_type, runtime_value_t f_value, runtime_type_t f_type, runtime_value_t context, runtime_type_t context_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, init_value_type);
	const auto& type2 = lookup_type_ref(backend, f_type);

//	QUARK_ASSERT(check_reduce_func_type(type0, type1, type2, lookup_type_ref(backend, f_type)));
	QUARK_ASSERT(is_vector_carray(backend.types, backend.config, type_t(elements_vec_type)));

	const auto& vec = *elements_vec.vector_carray_ptr;
	const auto& init = init_value;
	const auto f = reinterpret_cast<REDUCE_F>(f_value.function_ptr);

	auto count = vec.get_element_count();
	runtime_value_t acc = init;
	retain_value(backend, acc, type_t(init_value_type));

	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.get_element_ptr()[i];
		const auto acc2 = (*f)(frp, acc, element_value, context);

		if(is_rc_value(peek2(backend.types, type_t(init_value_type)))){
			release_value(backend, acc, type_t(init_value_type));
		}
		acc = acc2;
	}
	return acc;
}

//??? optimize prio 1
static runtime_value_t reduce__hamt(floyd_runtime_t* frp, value_backend_t& backend, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t init_value, runtime_type_t init_value_type, runtime_value_t f_value, runtime_type_t f_type, runtime_value_t context, runtime_type_t context_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, init_value_type);
	const auto& type2 = lookup_type_ref(backend, f_type);

//	QUARK_ASSERT(check_reduce_func_type(type0, type1, type2, lookup_type_ref(backend, f_type)));
	QUARK_ASSERT(is_vector_hamt(backend.types, backend.config, type_t(elements_vec_type)));

	const auto& vec = *elements_vec.vector_hamt_ptr;
	const auto& init = init_value;
	const auto f = reinterpret_cast<REDUCE_F>(f_value.function_ptr);

	auto count = vec.get_element_count();
	runtime_value_t acc = init;
	retain_value(backend, acc, type_t(init_value_type));

	for(int i = 0 ; i < count ; i++){
		const auto element_value = vec.load_element(i);
		const auto acc2 = (*f)(frp, acc, element_value, context);

		if(is_rc_value(peek2(backend.types, type_t(init_value_type)))){
			release_value(backend, acc, type_t(init_value_type));
		}
		acc = acc2;
	}
	return acc;
}

//	R reduce([E] elements, R accumulator_init, func R (R accumulator, E element, C context) f, C context)

//??? optimize prio 1
//??? check type at compile time, not runtime.
static runtime_value_t floyd_llvm_intrinsic__reduce(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, runtime_value_t init_value, runtime_type_t init_value_type, runtime_value_t f_value, runtime_type_t f_type, runtime_value_t context, runtime_type_t context_type){
	auto& r = get_floyd_runtime(frp);

	if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return reduce__carray(frp, r.backend, elements_vec, elements_vec_type, init_value, init_value_type, f_value, f_type, context, context_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return reduce__hamt(frp, r.backend, elements_vec, elements_vec_type, init_value, init_value_type, f_value, f_type, context, context_type);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}


/////////////////////////////////////////		stable_sort()



typedef uint8_t (*stable_sort_F)(floyd_runtime_t* frp, runtime_value_t left_value, runtime_value_t right_value, runtime_value_t context_value);

//??? optimize prio 1

static runtime_value_t stable_sort__carray(
	floyd_runtime_t* frp,
	value_backend_t& backend,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context_value,
	runtime_type_t context_value_type
){
	QUARK_ASSERT(frp != nullptr);
	QUARK_ASSERT(backend.check_invariant());

	const auto& types = backend.types;

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, f_value_type);
	const auto& type2 = lookup_type_ref(backend, context_value_type);

//	QUARK_ASSERT(check_stable_sort_func_type(type0, type1, type2));
	QUARK_ASSERT(is_vector_carray(types, backend.config, type_t(elements_vec_type)));

	const auto& elements = elements_vec;
	const auto& f = f_value;
	const auto& context = context_value;

	const auto elements2 = from_runtime_value2(backend, elements, type0);
	const auto f2 = reinterpret_cast<stable_sort_F>(f.function_ptr);

	struct sort_functor_r {
		bool operator() (const value_t &a, const value_t &b) {
			auto& r = get_floyd_runtime(frp);
			const auto left = to_runtime_value(r, a);
			const auto right = to_runtime_value(r, b);
			uint8_t result = (*f)(frp, left, right, context);
			return result == 1 ? true : false;
		}

		floyd_runtime_t* frp;
		runtime_value_t context;
		stable_sort_F f;
	};

	const sort_functor_r sort_functor { frp, context, f2 };

	auto mutate_inplace_elements = elements2.get_vector_value();
	std::stable_sort(mutate_inplace_elements.begin(), mutate_inplace_elements.end(), sort_functor);

	//??? optimize
	const auto result = to_runtime_value2(backend, value_t::make_vector_value(types, peek2(types, type0).get_vector_element_type(types), mutate_inplace_elements));
	return result;
}

//??? optimize prio 1

static runtime_value_t stable_sort__hamt(
	floyd_runtime_t* frp,
	value_backend_t& backend,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context_value,
	runtime_type_t context_value_type
){
	QUARK_ASSERT(frp != nullptr);
	QUARK_ASSERT(backend.check_invariant());

	const auto& types = backend.types;

	const auto& type0 = lookup_type_ref(backend, elements_vec_type);
	const auto& type1 = lookup_type_ref(backend, f_value_type);
	const auto& type2 = lookup_type_ref(backend, context_value_type);

//	QUARK_ASSERT(check_stable_sort_func_type(type0, type1, type2));
	QUARK_ASSERT(is_vector_hamt(types, backend.config, type_t(elements_vec_type)));

	const auto& elements = elements_vec;
	const auto& f = f_value;
	const auto& context = context_value;

	const auto elements2 = from_runtime_value2(backend, elements, type0);
	const auto f2 = reinterpret_cast<stable_sort_F>(f.function_ptr);

	struct sort_functor_r {
		bool operator() (const value_t &a, const value_t &b) {
			auto& r = get_floyd_runtime(frp);
			const auto left = to_runtime_value(r, a);
			const auto right = to_runtime_value(r, b);
			uint8_t result = (*f)(frp, left, right, context);
			return result == 1 ? true : false;
		}

		floyd_runtime_t* frp;
		runtime_value_t context;
		stable_sort_F f;
	};

	const sort_functor_r sort_functor { frp, context, f2 };

	auto mutate_inplace_elements = elements2.get_vector_value();
	std::stable_sort(mutate_inplace_elements.begin(), mutate_inplace_elements.end(), sort_functor);

	//??? optimize
	const auto result = to_runtime_value2(backend, value_t::make_vector_value(types, peek2(types, type0).get_vector_element_type(types), mutate_inplace_elements));
	return result;
}

//	[T] stable_sort([T] elements, bool less(T left, T right, C context), C context)

//??? optimize prio 1
//??? check type at compile time, not runtime.
static runtime_value_t floyd_llvm_intrinsic__stable_sort(
	floyd_runtime_t* frp,
	runtime_value_t elements_vec,
	runtime_type_t elements_vec_type,
	runtime_value_t f_value,
	runtime_type_t f_value_type,
	runtime_value_t context_value,
	runtime_type_t context_value_type
){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return stable_sort__carray(frp, r.backend, elements_vec, elements_vec_type, f_value, f_value_type, context_value, context_value_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return stable_sort__hamt(frp, r.backend, elements_vec, elements_vec_type, f_value, f_value_type, context_value, context_value_type);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		print()



//??? optimize prio 2

void floyd_llvm_intrinsic__print(floyd_runtime_t* frp, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto s = gen_to_string(r, value, value_type);
	printf("%s", s.c_str());

	const auto lines = split_on_chars(seq_t(s), "\n");
	r._print_output = concat(r._print_output, lines);
}




////////////////////////////////	push_back()



//??? Expensive to push_back since all elements in vector needs their RC bumped!
// Could specialize further, for vector_hamt<string>, vector_hamt<vector<x>> etc. But it's probably better to inline push_back() instead.

static runtime_value_t push_back__string(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type, runtime_value_t element){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, vec_type);
	QUARK_ASSERT(peek2(r.backend.types, type0).is_string());
#endif

	auto value = from_runtime_string(r, vec);
	value.push_back((char)element.int_value);
	const auto result2 = to_runtime_string(r, value);
	return result2;
}

//??? use memcpy()
static runtime_value_t floydrt_push_back_carray_pod(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type, runtime_value_t element){
	auto& r = get_floyd_runtime(frp);
#if DEBUG
	(void)r;
#endif

	const auto element_count = vec.vector_carray_ptr->get_element_count();
	auto source_ptr = vec.vector_carray_ptr->get_element_ptr();

	auto v2 = alloc_vector_carray(r.backend.heap, element_count + 1, element_count + 1, type_t(vec_type));
	auto dest_ptr = v2.vector_carray_ptr->get_element_ptr();
	for(int i = 0 ; i < element_count ; i++){
		dest_ptr[i] = source_ptr[i];
	}
	dest_ptr[element_count] = element;
	return v2;
}

static runtime_value_t floydrt_push_back_carray_nonpod(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type, runtime_value_t element){
	auto& r = get_floyd_runtime(frp);

	type_t element_itype = lookup_vector_element_type(r.backend, type_t(vec_type));
	const auto element_count = vec.vector_carray_ptr->get_element_count();
	auto source_ptr = vec.vector_carray_ptr->get_element_ptr();

	auto v2 = alloc_vector_carray(r.backend.heap, element_count + 1, element_count + 1, type_t(vec_type));
	auto dest_ptr = v2.vector_carray_ptr->get_element_ptr();
	for(int i = 0 ; i < element_count ; i++){
		dest_ptr[i] = source_ptr[i];
		retain_value(r.backend, dest_ptr[i], element_itype);
	}
	dest_ptr[element_count] = element;
	retain_value(r.backend, dest_ptr[element_count], element_itype);

	return v2;
}

static runtime_value_t floydrt_push_back_hamt_pod(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type, runtime_value_t element){
#if DEBUG
	auto& r = get_floyd_runtime(frp);
	(void)r;
#endif

	return push_back_immutable(vec, element);
}

static runtime_value_t floydrt_push_back_hamt_nonpod(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type, runtime_value_t element){
	auto& r = get_floyd_runtime(frp);

	runtime_value_t vec2 = push_back_immutable(vec, element);
	type_t element_itype = lookup_vector_element_type(r.backend, type_t(vec_type));

	for(int i = 0 ; i < vec2.vector_hamt_ptr->get_element_count() ; i++){
		const auto& value = vec2.vector_hamt_ptr->load_element(i);
		retain_value(r.backend, value, element_itype);
	}
	return vec2;
}


static std::vector<specialization_t> make_push_back_specializations(llvm::LLVMContext& context, const llvm_type_lookup& type_lookup){
	llvm::FunctionType* function_type = llvm::FunctionType::get(
		make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
		{
			make_frp_type(type_lookup),
			make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup),
			make_runtime_value_type(type_lookup)
		},
		false
	);
	return {
//		specialization_t { { "push_back", make_intrinsic_llvm_function_type(type_lookup, make_push_back_signature()), reinterpret_cast<void*>(floyd_llvm_intrinsic__push_back) }, xx),
		specialization_t { eresolved_type::k_string,				{ "push_back__string", function_type, reinterpret_cast<void*>(push_back__string) } },
		specialization_t { eresolved_type::k_vector_carray_pod,		{ "push_back_carray_pod", function_type, reinterpret_cast<void*>(floydrt_push_back_carray_pod) } },
		specialization_t { eresolved_type::k_vector_carray_nonpod,	{ "push_back_carray_nonpod", function_type, reinterpret_cast<void*>(floydrt_push_back_carray_nonpod) } },
		specialization_t { eresolved_type::k_vector_hamt_pod,		{ "push_back_hamt_pod", function_type, reinterpret_cast<void*>(floydrt_push_back_hamt_pod) } },
		specialization_t { eresolved_type::k_vector_hamt_nonpod, 	{ "push_back_hamt_nonpod", function_type, reinterpret_cast<void*>(floydrt_push_back_hamt_nonpod) } }
	};
}

llvm::Value* generate_instrinsic_push_back(llvm_function_generator_t& gen_acc, const type_t& resolved_call_type, llvm::Value& collection_reg, const type_t& collection_type, llvm::Value& value_reg){
	QUARK_ASSERT(gen_acc.check_invariant());
	QUARK_ASSERT(collection_type.check_invariant());

	auto& builder = gen_acc.get_builder();
	const auto& types = gen_acc.gen.type_lookup.state.types;

	const auto res = lookup_link_map(gen_acc.gen.settings.config, types, gen_acc.gen.link_map, make_push_back_specializations(builder.getContext(), gen_acc.gen.type_lookup), collection_type);
	const auto collection_type_peek = peek2(types, collection_type);

	if(collection_type_peek.is_string()){
		const auto vector_itype_reg = generate_itype_constant(gen_acc.gen, collection_type);
		const auto packed_value_reg = generate_cast_to_runtime_value(gen_acc.gen, value_reg, type_t::make_int());
		return builder.CreateCall(
			res.llvm_codegen_f,
			{ gen_acc.get_callers_fcp(), &collection_reg, vector_itype_reg, packed_value_reg },
			""
		);
	}
	else if(collection_type_peek.is_vector()){
		const auto element_type = collection_type_peek.get_vector_element_type(types);
		const auto vector_itype_reg = generate_itype_constant(gen_acc.gen, collection_type);
		const auto packed_value_reg = generate_cast_to_runtime_value(gen_acc.gen, value_reg, element_type);
		return builder.CreateCall(
			res.llvm_codegen_f,
			{ gen_acc.get_callers_fcp(), &collection_reg, vector_itype_reg, packed_value_reg },
			""
		);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}




////////////////////////////////	replace()



//??? optimize prio 1
//??? check type at compile time, not runtime.

//	replace(VECTOR s, int start, int end, VECTOR new)	
static const runtime_value_t floyd_llvm_intrinsic__replace(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, uint64_t start, uint64_t end, runtime_value_t arg3_value, runtime_type_t arg3_type){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	const auto& type3 = lookup_type_ref(r.backend, arg3_type);

	QUARK_ASSERT(type3 == type0);

	if(peek2(r.backend.types, type0).is_string()){
		return replace__string(r.backend, elements_vec, elements_vec_type, start, end, arg3_value, arg3_type);
	}
	else if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return replace__carray(r.backend, elements_vec, elements_vec_type, start, end, arg3_value, arg3_type);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return replace__hamt(r.backend, elements_vec, elements_vec_type, start, end, arg3_value, arg3_type);
	}
	else{
		//	No other types allowed.
		UNSUPPORTED();
	}
}




////////////////////////////////	parse_json_script()




static JSON_T* floyd_llvm_intrinsic__parse_json_script(floyd_runtime_t* frp, runtime_value_t string_s0){
	auto& r = get_floyd_runtime(frp);

	const auto string_s = from_runtime_string(r, string_s0);

	std::pair<json_t, seq_t> result0 = parse_json(seq_t(string_s));
	auto result = alloc_json(r.backend.heap, result0.first);
	return result;
}


/////////////////////////////////////////		send()


//??? optimize prio 2

static void floyd_llvm_intrinsic__send(floyd_runtime_t* frp, runtime_value_t process_id0, const JSON_T* message_json_ptr){
	auto& r = get_floyd_runtime(frp);

	QUARK_ASSERT(message_json_ptr != nullptr);

	const auto& process_id = from_runtime_string(r, process_id0);
	const auto& message_json = message_json_ptr->get_json();

	if(k_trace_process_messaging){
		QUARK_TRACE_SS("send(\"" << process_id << "\"," << json_to_pretty_string(message_json) <<")");
	}

	r._handler->on_send(process_id, message_json);
}




////////////////////////////////	size()



static int64_t size__string(floyd_runtime_t* frp, runtime_value_t vec, runtime_type_t vec_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, vec_type);
	QUARK_ASSERT(peek2(r.backend.types, type0).is_string());
#endif

	return vec.vector_carray_ptr->get_element_count();
}

static int64_t size_vector_carray(floyd_runtime_t* frp, runtime_value_t collection, runtime_type_t collection_type){
	auto& r = get_floyd_runtime(frp);
	(void)r;
	return collection.vector_carray_ptr->get_element_count();
}
static int64_t size_vector_hamt(floyd_runtime_t* frp, runtime_value_t collection, runtime_type_t collection_type){
	auto& r = get_floyd_runtime(frp);
	(void)r;
	return collection.vector_hamt_ptr->get_element_count();
}
static int64_t size_dict_cppmap(floyd_runtime_t* frp, runtime_value_t collection, runtime_type_t collection_type){
	auto& r = get_floyd_runtime(frp);
	(void)r;
	return collection.dict_cppmap_ptr->size();
}
static int64_t size_dict_hamt(floyd_runtime_t* frp, runtime_value_t collection, runtime_type_t collection_type){
	auto& r = get_floyd_runtime(frp);
	(void)r;
	return collection.dict_hamt_ptr->size();
}
static int64_t size_json(floyd_runtime_t* frp, runtime_value_t collection, runtime_type_t collection_type){
	auto& r = get_floyd_runtime(frp);
	(void)r;

	const auto& json = collection.json_ptr->get_json();

	if(json.is_object()){
		return json.get_object_size();
	}
	else if(json.is_array()){
		return json.get_array_size();
	}
	else if(json.is_string()){
		return json.get_string().size();
	}
	else{
		quark::throw_runtime_error("Calling size() on unsupported type of value.");
	}
}

static std::vector<specialization_t> make_size_specializations(llvm::LLVMContext& context, const llvm_type_lookup& type_lookup){
	llvm::FunctionType* function_type1 = llvm::FunctionType::get(
		llvm::Type::getInt64Ty(context),
		{
			make_frp_type(type_lookup),
			make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup)
		},
		false
	);
	llvm::FunctionType* function_type2 = llvm::FunctionType::get(
		llvm::Type::getInt64Ty(context),
		{
			make_frp_type(type_lookup),
			make_generic_dict_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup)
		},
		false
	);
	llvm::FunctionType* function_type3 = llvm::FunctionType::get(
		llvm::Type::getInt64Ty(context),
		{
			make_frp_type(type_lookup),
			make_json_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup)
		},
		false
	);
	return {
//		specialization_t { eresolved_type::k_string,					{ "size", make_intrinsic_llvm_function_type(type_lookup, make_size_signature()), reinterpret_cast<void*>(floyd_llvm_intrinsic__size) },
		specialization_t { eresolved_type::k_string,					{ "size__string", function_type1, reinterpret_cast<void*>(size__string) } },

		specialization_t { eresolved_type::k_vector_carray_pod,			{ "size_vector_carray", function_type1, reinterpret_cast<void*>(size_vector_carray) } },
		specialization_t { eresolved_type::k_vector_carray_nonpod,		{ "size_vector_carray", function_type1, reinterpret_cast<void*>(size_vector_carray) } },
		specialization_t { eresolved_type::k_vector_hamt_pod,			{ "size_vector_hamt", function_type1, reinterpret_cast<void*>(size_vector_hamt) } },
		specialization_t { eresolved_type::k_vector_hamt_nonpod,		{ "size_vector_hamt", function_type1, reinterpret_cast<void*>(size_vector_hamt) } },

		specialization_t { eresolved_type::k_dict_cppmap_pod,			{ "size_dict_cppmap", function_type2, reinterpret_cast<void*>(size_dict_cppmap) } },
		specialization_t { eresolved_type::k_dict_cppmap_nonpod,		{ "size_dict_cppmap", function_type2, reinterpret_cast<void*>(size_dict_cppmap) } },
		specialization_t { eresolved_type::k_dict_hamt_pod,				{ "size_dict_hamt", function_type2, reinterpret_cast<void*>(size_dict_hamt) } },
		specialization_t { eresolved_type::k_dict_hamt_nonpod,			{ "size_dict_hamt", function_type2, reinterpret_cast<void*>(size_dict_hamt) } },

		specialization_t { eresolved_type::k_json,						{ "size_json", function_type3, reinterpret_cast<void*>(size_json) } }
	};
}

llvm::Value* generate_instrinsic_size(llvm_function_generator_t& gen_acc, const type_t& resolved_call_type, llvm::Value& collection_reg, const type_t& collection_type){
	QUARK_ASSERT(gen_acc.check_invariant());
	QUARK_ASSERT(collection_type.check_invariant());

	auto& builder = gen_acc.get_builder();
	const auto& types = gen_acc.gen.type_lookup.state.types;

	const auto res = lookup_link_map(gen_acc.gen.settings.config, types, gen_acc.gen.link_map, make_size_specializations(builder.getContext(), gen_acc.gen.type_lookup), collection_type);
	const auto collection_itype = generate_itype_constant(gen_acc.gen, collection_type);
	return builder.CreateCall(
		res.llvm_codegen_f,
		{ gen_acc.get_callers_fcp(), &collection_reg, collection_itype },
		""
	);
}



/////////////////////////////////////////		subset()


//??? optimize prio 1
//??? check type at compile time, not runtime.

// VECTOR subset(VECTOR s, int start, int end)
static const runtime_value_t floyd_llvm_intrinsic__subset(floyd_runtime_t* frp, runtime_value_t elements_vec, runtime_type_t elements_vec_type, uint64_t start, uint64_t end){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, elements_vec_type);
	if(peek2(r.backend.types, type0).is_string()){
		return subset__string(r.backend, elements_vec, elements_vec_type, start, end);
	}
	else if(is_vector_carray(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return subset__carray(r.backend, elements_vec, elements_vec_type, start, end);
	}
	else if(is_vector_hamt(r.backend.types, r.backend.config, type_t(elements_vec_type))){
		return subset__hamt(r.backend, elements_vec, elements_vec_type, start, end);
	}
	else{
		//	No other types allowed.
		UNSUPPORTED();
	}
}


/////////////////////////////////////////		to_pretty_string()


static runtime_value_t floyd_llvm_intrinsic__to_pretty_string(floyd_runtime_t* frp, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, value_type);
	const auto& value2 = from_runtime_value(r, value, type0);
	const auto json = value_to_ast_json(r.backend.types, value2);
	const auto s = json_to_pretty_string(json, 0, pretty_t{ 80, 4 });
	return to_runtime_string(r, s);
}



/////////////////////////////////////////		to_string()



//??? optimize prio 2
static runtime_value_t floyd_llvm_intrinsic__to_string(floyd_runtime_t* frp, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto s = gen_to_string(r, value, value_type);
	return to_runtime_string(r, s);
}



/////////////////////////////////////////		typeof()



static runtime_type_t floyd_llvm_intrinsic__typeof(floyd_runtime_t* frp, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, value_type);
	QUARK_ASSERT(type0.check_invariant());
#endif
	return value_type;
}



/////////////////////////////////////////		update()




static const runtime_value_t update_string(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);
#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type0).is_string());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_int());
	QUARK_ASSERT(peek2(r.backend.types, type2).is_int());
#endif
	return update__string(r.backend, coll_value, key_value, value);
}


static const runtime_value_t update_vector_carray_pod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_int());
#endif

	return update__vector_carray(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_vector_carray_nonpod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_int());
#endif

	return update__vector_carray(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_vector_hamt_pod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_int());
#endif

	return update__vector_hamt_pod(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_vector_hamt_nonpod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_int());
#endif

	return update__vector_hamt_nonpod(r.backend, coll_value, coll_type, key_value, value);
}


static const runtime_value_t update_dict_cppmap_pod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_string());
#endif

	return update__dict_cppmap(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_dict_cppmap_nonpod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_string());
#endif

	return update__dict_cppmap(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_dict_hamt_pod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_string());
#endif
	return update__dict_hamt(r.backend, coll_value, coll_type, key_value, value);
}
static const runtime_value_t update_dict_hamt_nonpod(floyd_runtime_t* frp, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

#if DEBUG
	const auto& type0 = lookup_type_ref(r.backend, coll_type);
	const auto& type1 = lookup_type_ref(r.backend, key_type);
	const auto& type2 = lookup_type_ref(r.backend, value_type);

	QUARK_ASSERT(type0.check_invariant());
	QUARK_ASSERT(type1.check_invariant());
	QUARK_ASSERT(type2.check_invariant());
	QUARK_ASSERT(peek2(r.backend.types, type1).is_string());
#endif
	return update__dict_hamt(r.backend, coll_value, coll_type, key_value, value);
}

static std::vector<specialization_t> make_update_specializations(llvm::LLVMContext& context, const llvm_type_lookup& type_lookup){
	llvm::FunctionType* function_type1 = llvm::FunctionType::get(
		make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
		{
			make_frp_type(type_lookup),

			make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup),

			llvm::Type::getInt64Ty(context),
			make_runtime_type_type(type_lookup),

			make_runtime_value_type(type_lookup),
			make_runtime_type_type(type_lookup),
		},
		false
	);
	llvm::FunctionType* function_type2 = llvm::FunctionType::get(
		make_generic_dict_type_byvalue(type_lookup)->getPointerTo(),
		{
			make_frp_type(type_lookup),

			make_generic_dict_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup),

			make_generic_vec_type_byvalue(type_lookup)->getPointerTo(),
			make_runtime_type_type(type_lookup),

			make_runtime_value_type(type_lookup),
			make_runtime_type_type(type_lookup)
		},
		false
	);
	return {
//		specialization_t{ eresolved_type::k_string, { "update", make_intrinsic_llvm_function_type(type_lookup, make_update_signature()), reinterpret_cast<void*>(floyd_llvm_intrinsic__update) } }
		specialization_t { eresolved_type::k_string,					{ "update__string", function_type1, reinterpret_cast<void*>(update_string) } },

		specialization_t { eresolved_type::k_vector_carray_pod,			{ "update_vector_carray", function_type1, reinterpret_cast<void*>(update_vector_carray_pod) } },
		specialization_t { eresolved_type::k_vector_carray_nonpod,		{ "update_vector_carray", function_type1, reinterpret_cast<void*>(update_vector_carray_nonpod) } },
		specialization_t { eresolved_type::k_vector_hamt_pod,			{ "update_vector_hamt", function_type1, reinterpret_cast<void*>(update_vector_hamt_pod) } },
		specialization_t { eresolved_type::k_vector_hamt_nonpod,		{ "update_vector_hamt", function_type1, reinterpret_cast<void*>(update_vector_hamt_nonpod) } },

		specialization_t { eresolved_type::k_dict_cppmap_pod,			{ "update_dict_cppmap", function_type2, reinterpret_cast<void*>(update_dict_cppmap_pod) } },
		specialization_t { eresolved_type::k_dict_cppmap_nonpod,		{ "update_dict_cppmap", function_type2, reinterpret_cast<void*>(update_dict_cppmap_nonpod) } },
		specialization_t { eresolved_type::k_dict_hamt_pod,				{ "update_dict_hamt", function_type2, reinterpret_cast<void*>(update_dict_hamt_pod) } },
		specialization_t { eresolved_type::k_dict_hamt_nonpod,			{ "update_dict_hamt", function_type2, reinterpret_cast<void*>(update_dict_hamt_nonpod) } },
	};
}

llvm::Value* generate_instrinsic_update(llvm_function_generator_t& gen_acc, const type_t& resolved_call_type, llvm::Value& collection_reg, const type_t& collection_type, llvm::Value& key_reg, llvm::Value& value_reg){
	QUARK_ASSERT(collection_type.check_invariant());

	auto& builder = gen_acc.get_builder();
	const auto& types = gen_acc.gen.type_lookup.state.types;

	const auto res = lookup_link_map(
		gen_acc.gen.settings.config,
		types,
		gen_acc.gen.link_map,
		make_update_specializations(builder.getContext(), gen_acc.gen.type_lookup),
		collection_type
	);
	const auto collection_itype = generate_itype_constant(gen_acc.gen, collection_type);
	const auto collection_type_peek = peek2(types, collection_type);

	if(collection_type_peek.is_string()){
		const auto key_itype = generate_itype_constant(gen_acc.gen, type_t::make_int());
		const auto value_itype = generate_itype_constant(gen_acc.gen, type_t::make_int());
		return builder.CreateCall(
			res.llvm_codegen_f,
			{ gen_acc.get_callers_fcp(), &collection_reg, collection_itype, &key_reg, key_itype, &value_reg, value_itype },
			""
		);
	}
	else if(collection_type_peek.is_vector()){
		const auto key_itype = generate_itype_constant(gen_acc.gen, type_t::make_int());
		const auto element_type = collection_type_peek.get_vector_element_type(types);

		const auto packed_value_reg = generate_cast_to_runtime_value(gen_acc.gen, value_reg, element_type);
		const auto value_itype = generate_itype_constant(gen_acc.gen, element_type);
		return builder.CreateCall(
			res.llvm_codegen_f,
			{ gen_acc.get_callers_fcp(), &collection_reg, collection_itype, &key_reg, key_itype, packed_value_reg, value_itype },
			""
		);
	}
	else if(collection_type_peek.is_dict()){
		const auto key_itype = generate_itype_constant(gen_acc.gen, type_t::make_string());
		const auto element_type = collection_type_peek.get_dict_value_type(types);
		const auto value_itype = generate_itype_constant(gen_acc.gen, element_type);
		const auto packed_value_reg = generate_cast_to_runtime_value(gen_acc.gen, value_reg, element_type);
		return builder.CreateCall(
			res.llvm_codegen_f,
			{ gen_acc.get_callers_fcp(), &collection_reg, collection_itype, &key_reg, key_itype, packed_value_reg, value_itype },
			""
		);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



/////////////////////////////////////////		to_json()



static JSON_T* floyd_llvm_intrinsic__to_json(floyd_runtime_t* frp, runtime_value_t value, runtime_type_t value_type){
	auto& r = get_floyd_runtime(frp);

	const auto& type0 = lookup_type_ref(r.backend, value_type);
	const auto value0 = from_runtime_value(r, value, type0);
	const auto j = value_to_ast_json(r.backend.types, value0);
	auto result = alloc_json(r.backend.heap, j);
	return result;
}





/////////////////////////////////////////		REGISTRY




static std::map<std::string, void*> get_intrinsic_binds(){

	const std::map<std::string, void*> binds = {
		{ "assert", reinterpret_cast<void *>(&floyd_llvm_intrinsic__assert) },
		{ "to_string", reinterpret_cast<void *>(&floyd_llvm_intrinsic__to_string) },
		{ "to_pretty_string", reinterpret_cast<void *>(&floyd_llvm_intrinsic__to_pretty_string) },
		{ "typeof", reinterpret_cast<void *>(&floyd_llvm_intrinsic__typeof) },

//		{ "update", reinterpret_cast<void *>(&floyd_llvm_intrinsic__update) },
//		{ "size", reinterpret_cast<void *>(&floyd_llvm_intrinsic__size) },
		{ "find", reinterpret_cast<void *>(&floyd_llvm_intrinsic__find) },
		{ "exists", reinterpret_cast<void *>(&floyd_llvm_intrinsic__exists) },
		{ "erase", reinterpret_cast<void *>(&floyd_llvm_intrinsic__erase) },
		{ "get_keys", reinterpret_cast<void *>(&floyd_llvm_intrinsic__get_keys) },
//		{ "push_back", reinterpret_cast<void *>(&floyd_llvm_intrinsic__push_back) },
		{ "subset", reinterpret_cast<void *>(&floyd_llvm_intrinsic__subset) },
		{ "replace", reinterpret_cast<void *>(&floyd_llvm_intrinsic__replace) },

		{ "generate_json_script", reinterpret_cast<void *>(&floyd_llvm_intrinsic__generate_json_script) },
		{ "from_json", reinterpret_cast<void *>(&floyd_llvm_intrinsic__from_json) },
		{ "parse_json_script", reinterpret_cast<void *>(&floyd_llvm_intrinsic__parse_json_script) },
		{ "to_json", reinterpret_cast<void *>(&floyd_llvm_intrinsic__to_json) },

		{ "get_json_type", reinterpret_cast<void *>(&floyd_llvm_intrinsic__get_json_type) },

//		{ "map", reinterpret_cast<void *>(&floyd_llvm_intrinsic__map) },
//		{ "map_string", reinterpret_cast<void *>(&floyd_llvm_intrinsic__map_string) },
		{ "map_dag", reinterpret_cast<void *>(&floyd_llvm_intrinsic__map_dag) },
		{ "filter", reinterpret_cast<void *>(&floyd_llvm_intrinsic__filter) },
		{ "reduce", reinterpret_cast<void *>(&floyd_llvm_intrinsic__reduce) },
		{ "stable_sort", reinterpret_cast<void *>(&floyd_llvm_intrinsic__stable_sort) },

		{ "print", reinterpret_cast<void *>(&floyd_llvm_intrinsic__print) },
		{ "send", reinterpret_cast<void *>(&floyd_llvm_intrinsic__send) },

/*
		{ "bw_not", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_and", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_or", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_xor", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_shift_left", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_shift_right", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
		{ "bw_shift_right_arithmetic", reinterpret_cast<void *>(&floyd_llvm_intrinsic__dummy) },
*/
	};
	return binds;
}

//	Skips duplicates.
static std::vector<function_link_entry_t> make_entries(const intrinsic_signatures_t& intrinsic_signatures, const std::vector<function_bind_t>& binds){
	std::vector<function_link_entry_t> result;
	for(const auto& bind: binds){
		auto signature_it = std::find_if(intrinsic_signatures.vec.begin(), intrinsic_signatures.vec.end(), [&] (const intrinsic_signature_t& m) { return m.name == bind.name; } );
		const auto function_type = signature_it != intrinsic_signatures.vec.end() ? signature_it->_function_type : make_undefined();

		const auto link_name = encode_intrinsic_link_name(bind.name);
		const auto exists_it = std::find_if(result.begin(), result.end(), [&](const function_link_entry_t& e){ return e.link_name == link_name; });

		if(exists_it == result.end()){
			QUARK_ASSERT(bind.llvm_function_type != nullptr);
			const auto def = function_link_entry_t{ "intrinsic", link_name, bind.llvm_function_type, nullptr, function_type, {}, bind.native_f };
			result.push_back(def);
		}
	}
	return result;
}

static std::vector<function_link_entry_t> make_entries2(const intrinsic_signatures_t& intrinsic_signatures, const std::vector<specialization_t>& specializations){
	const auto binds = mapf<function_bind_t>(specializations, [](auto& e){ return e.bind; });
	return make_entries(intrinsic_signatures, binds);
}

std::vector<function_link_entry_t> make_intrinsics_link_map(llvm::LLVMContext& context, const llvm_type_lookup& type_lookup, const intrinsic_signatures_t& intrinsic_signatures){
	QUARK_ASSERT(type_lookup.check_invariant());

	const auto& types = type_lookup.state.types;

	const auto binds = get_intrinsic_binds();

	std::vector<function_link_entry_t> result;
	for(const auto& bind: binds){
		auto signature_it = std::find_if(intrinsic_signatures.vec.begin(), intrinsic_signatures.vec.end(), [&] (const intrinsic_signature_t& e) { return e.name == bind.first; } );
		QUARK_ASSERT(signature_it != intrinsic_signatures.vec.end());

		const auto link_name = encode_intrinsic_link_name(bind.first);
		const auto function_type = signature_it->_function_type;
		llvm::Type* function_ptr_type = get_llvm_type_as_arg(type_lookup, signature_it->_function_type);
		const auto function_byvalue_type = deref_ptr(function_ptr_type);
		const auto def = function_link_entry_t{ "intrinsic", link_name, (llvm::FunctionType*)function_byvalue_type, nullptr, function_type, {}, bind.second };
		result.push_back(def);
	}

	result = concat(result, make_entries2(intrinsic_signatures, make_push_back_specializations(context, type_lookup)));
	result = concat(result, make_entries2(intrinsic_signatures, make_size_specializations(context, type_lookup)));
	result = concat(result, make_entries2(intrinsic_signatures, make_update_specializations(context, type_lookup)));
	result = concat(result, make_entries2(intrinsic_signatures, make_map_specializations(context, type_lookup)));

	if(k_trace_function_link_map){
		trace_function_link_map(types, result);
	}

	return result;
}


} // floyd
