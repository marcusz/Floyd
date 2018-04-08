//
//  parser_evaluator.h
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 26/07/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#ifndef bytecode_interpreter_hpp
#define bytecode_interpreter_hpp


#include "quark.h"

#include <string>
#include <vector>
#include <map>
#include "ast_value.h"//??? remove
#include "ast_typeid.h"//??? remove
#include "json_support.h"
#include "statement.h"

//??? remove usage of symbol_t
//??? remove usage of typeid_t
//??? remove usage of value_t

namespace floyd {
	struct value_t;
	struct bc_value_t;
	struct interpreter_t;
	struct bc_program_t;
	struct bc_frame_t;


	typedef value_t (*HOST_FUNCTION_PTR)(interpreter_t& vm, const std::vector<value_t>& args);

	typedef int16_t bc_typeid_t;


	//////////////////////////////////////		value_t -- helpers


	value_t bc_to_value(const bc_value_t& value, const typeid_t& type);
	bc_value_t value_to_bc(const value_t& value);

	std::vector<bc_value_t> values_to_bcs(const std::vector<value_t>& values);
	std::vector<value_t> bcs_to_values__same_types(const std::vector<bc_value_t>& values, const typeid_t& shared_type);


	//////////////////////////////////////		bc_value_object_t

	/*
		This object contains the internals of values too big to be stored directly inside bc_value_t / bc_pod_value_t.
	*/

	struct bc_value_object_t {
#if DEBUG
		public: bool check_invariant() const{
			QUARK_ASSERT(_rc > 0);
			QUARK_ASSERT(_type.check_invariant());
			QUARK_ASSERT(_typeid_value.check_invariant());

			const auto base_type = _type.get_base_type();
			if(base_type == base_type::k_string){
//				QUARK_ASSERT(_string);
				QUARK_ASSERT(_json_value == nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
				QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());
			}
			else if(base_type == base_type::k_json_value){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value != nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
				QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());

				QUARK_ASSERT(_json_value->check_invariant());
			}

			else if(base_type == base_type::k_typeid){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value == nullptr);
		//		QUARK_ASSERT(_typeid_value != typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
				QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());

				QUARK_ASSERT(_typeid_value.check_invariant());
			}
			else if(base_type == base_type::k_struct){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value == nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
//				QUARK_ASSERT(_struct != nullptr);
				QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());

//				QUARK_ASSERT(_struct && _struct->check_invariant());
			}
			else if(base_type == base_type::k_vector){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value == nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
		//		QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());
			}
			else if(base_type == base_type::k_dict){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value == nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
				QUARK_ASSERT(_vector_elements.empty());
		//		QUARK_ASSERT(_dict_entries.empty());
			}
			else if(base_type == base_type::k_function){
				QUARK_ASSERT(_string.empty());
				QUARK_ASSERT(_json_value == nullptr);
				QUARK_ASSERT(_typeid_value == typeid_t::make_undefined());
				QUARK_ASSERT(_struct_members.empty());
				QUARK_ASSERT(_vector_elements.empty());
				QUARK_ASSERT(_dict_entries.empty());
			}
			else {
				QUARK_ASSERT(false);
			}
			return true;
		}
#endif

		public: bool operator==(const bc_value_object_t& other) const;


		public: bc_value_object_t(const std::string& s) :
			_rc(1),
#if DEBUG
			_type(typeid_t::make_string()),
#endif
			_string(s)
		{
			QUARK_ASSERT(check_invariant());
		}

		public: bc_value_object_t(const std::shared_ptr<json_t>& s) :
			_rc(1),
#if DEBUG
			_type(typeid_t::make_json_value()),
#endif
			_json_value(s)
		{
			QUARK_ASSERT(check_invariant());
		}

		public: bc_value_object_t(const typeid_t& s) :
			_rc(1),
#if DEBUG
			_type(typeid_t::make_typeid()),
#endif
			_typeid_value(s)
		{
			QUARK_ASSERT(check_invariant());
		}

		public: bc_value_object_t(const typeid_t& type, const std::vector<bc_value_t>& s, bool struct_tag) :
			_rc(1),
#if DEBUG
			_type(type),
#endif
			_struct_members(s)
		{
			QUARK_ASSERT(check_invariant());
		}
		public: bc_value_object_t(const typeid_t& type, const std::vector<bc_value_t>& s) :
			_rc(1),
#if DEBUG
			_type(type),
#endif
			_vector_elements(s)
		{
			QUARK_ASSERT(check_invariant());
		}
		public: bc_value_object_t(const typeid_t& type, const std::map<std::string, bc_value_t>& s) :
			_rc(1),
#if DEBUG
			_type(type),
#endif
			_dict_entries(s)
		{
			QUARK_ASSERT(check_invariant());
		}


		public: mutable int _rc;
		public: bool _is_unwritten_ext_value = false;
#if DEBUG
//??? use bc_typeid_t instead
		public: typeid_t _type;
#endif
		//	Holds ALL variants of objects right now -- optimize!
		public: std::string _string;
		public: std::shared_ptr<json_t> _json_value;
		public: typeid_t _typeid_value = typeid_t::make_undefined();
		public: std::vector<bc_value_t> _struct_members;
		public: std::vector<bc_value_t> _vector_elements;
		public: std::map<std::string, bc_value_t> _dict_entries;
	};



	//////////////////////////////////////		bc_pod_value_t



	//	IMPORTANT: Has no constructor, destructor etc!! POD.


	union bc_pod_value_t {
		bool _bool;
		int _int;
		float _float;
		int _function_id;
		bc_value_object_t* _ext;
		const bc_frame_t* _frame_ptr;
	};



	//////////////////////////////////////		bc_value_t

	/*
		Efficient value-object. Holds intern values or RC-objects. Handles RC automatically.
	*/

	struct bc_value_t {
		static inline void release_ext(bc_value_object_t* ext){
			ext->_rc--;
			if(ext->_rc == 0){
				delete ext;
				ext = nullptr;
			}
		}
		static inline void release_ext_pod(bc_pod_value_t& value){
			QUARK_ASSERT(value._ext != nullptr);

			value._ext->_rc--;
			if(value._ext->_rc == 0){
				delete value._ext;
				value._ext = nullptr;
			}
		}


		//	??? very slow?
		//	Will this type of value require an ext ? bc_value_object_t to be used?
		inline static bool is_bc_ext(base_type basetype){
			return false
				|| basetype == base_type::k_string
				|| basetype == base_type::k_json_value
				|| basetype == base_type::k_typeid
				|| basetype == base_type::k_struct
				|| basetype == base_type::k_vector
				|| basetype == base_type::k_dict
				;
		}


		public: bc_value_t() :
#if DEBUG
			_debug_type(typeid_t::make_undefined()),
#endif
			_is_ext(false)
		{
			_pod._ext = nullptr;
			QUARK_ASSERT(check_invariant());
		}

		public: ~bc_value_t(){
			QUARK_ASSERT(check_invariant());

			if(_is_ext){
				release_ext_pod(_pod);
			}
		}

		public: bc_value_t(const bc_value_t& other) :
#if DEBUG
			_debug_type(other._debug_type),
#endif
			_is_ext(other._is_ext),
			_pod(other._pod)
		{
			QUARK_ASSERT(other.check_invariant());

			if(_is_ext){
				_pod._ext->_rc++;
			}

			QUARK_ASSERT(check_invariant());
		}

		public: bc_value_t& operator=(const bc_value_t& other){
			QUARK_ASSERT(other.check_invariant());
			QUARK_ASSERT(check_invariant());

			bc_value_t temp(other);
			temp.swap(*this);

			QUARK_ASSERT(other.check_invariant());
			QUARK_ASSERT(check_invariant());
			return *this;
		}

/*
		public: bool operator==(const bc_value_t& other) const{
			return compare_value_true_deep(*this, other) == 0;
		}
*/
/*
		public: bool operator==(const bc_value_t& other) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(other.check_invariant());

			if(_is_ext != other._is_ext){
				return false;
			}

			if(_basetype == base_type::k_internal_undefined){
				return true;
			}
			else if(_basetype == base_type::k_bool){
				return _pod._bool == other._pod._bool;
			}
			else if(_basetype == base_type::k_int){
				return _pod._int == other._pod._int;
			}
			else if(_basetype == base_type::k_float){
				return _pod._float == other._pod._float;
			}
			else{
				QUARK_ASSERT(is_ext(_basetype));
				return compare_shared_values(_pod._ext, other._pod._ext);
			}
		}

		public: bool operator!=(const bc_value_t& other) const{
			return !(*this == other);
		}
*/

		public: void swap(bc_value_t& other){
			QUARK_ASSERT(other.check_invariant());
			QUARK_ASSERT(check_invariant());

#if DEBUG
			std::swap(_debug_type, other._debug_type);
#endif
			std::swap(_is_ext, other._is_ext);
			std::swap(_pod, other._pod);

			QUARK_ASSERT(other.check_invariant());
			QUARK_ASSERT(check_invariant());
		}

		private: static bc_value_t from_value(const value_t& value){
			QUARK_ASSERT(value.check_invariant());

			const auto basetype = value.get_basetype();
			if(basetype == base_type::k_internal_undefined){
				return make_undefined();
			}
			else if(basetype == base_type::k_internal_dynamic){
				return make_internal_dynamic();
			}
			else if(basetype == base_type::k_void){
				return make_void();
			}
			else if(basetype == base_type::k_bool){
				return make_bool(value.get_bool_value());
			}
			else if(basetype == base_type::k_bool){
				return make_bool(value.get_bool_value());
			}
			else if(basetype == base_type::k_int){
				return make_int(value.get_int_value());
			}
			else if(basetype == base_type::k_float){
				return make_float(value.get_float_value());
			}

			else if(basetype == base_type::k_string){
				return make_string(value.get_string_value());
			}
			else if(basetype == base_type::k_json_value){
				return make_json_value(value.get_json_value());
			}
			else if(basetype == base_type::k_typeid){
				return make_typeid_value(value.get_typeid_value());
			}
			else if(basetype == base_type::k_struct){
				return make_struct_value(value.get_type(), values_to_bcs(value.get_struct_value()->_member_values));
			}

			else if(basetype == base_type::k_vector){
				return make_vector_value(value.get_type().get_vector_element_type(), values_to_bcs(value.get_vector_value()));
			}
			else if(basetype == base_type::k_dict){
				const auto elements = value.get_dict_value();
				std::map<std::string, bc_value_t> entries2;
				for(const auto e: elements){
					entries2.insert({e.first, value_to_bc(e.second)});
				}
				return make_dict_value(value.get_type().get_dict_value_type(), entries2);
			}
			else if(basetype == base_type::k_function){
				return make_function_value(value.get_type(), value.get_function_value());
			}
			else{
				QUARK_ASSERT(false);
				throw std::exception();
			}
		}

#if DEBUG
		public: bool check_invariant() const {
			QUARK_ASSERT(_debug_type.check_invariant());
			return true;
		}
#endif

		public: explicit bc_value_t(const bc_frame_t* frame_ptr) :
#if DEBUG
			_debug_type(typeid_t::make_void()),
#endif
			_is_ext(false)
		{
			_pod._frame_ptr = frame_ptr;
			QUARK_ASSERT(check_invariant());
		}

		enum class mode {
			k_unwritten_ext_value
		};
		public: inline  explicit bc_value_t(const typeid_t& type, mode mode) :
#if DEBUG
			_debug_type(type),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{"UNWRITTEN EXT VALUE"};
			_pod._ext->_is_unwritten_ext_value = true;
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		internal-undefined type


		public: static bc_value_t make_undefined(){
			return bc_value_t();
		}


		//////////////////////////////////////		internal-dynamic type


		public: static bc_value_t make_internal_dynamic(){
			return bc_value_t();
		}

		//////////////////////////////////////		void


		public: static bc_value_t make_void(){
			return bc_value_t();
		}


		//////////////////////////////////////		bool


		public: inline static bc_value_t make_bool(bool v){
			return bc_value_t(v);
		}
		public: inline bool get_bool_value() const {
			QUARK_ASSERT(check_invariant());

			return _pod._bool;
		}
		private: inline explicit bc_value_t(bool value) :
#if DEBUG
			_debug_type(typeid_t::make_bool()),
#endif
			_is_ext(false)
		{
			_pod._bool = value;
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		int


		public: inline static bc_value_t make_int(int v){
			return bc_value_t{ v };
		}
		public: inline int get_int_value() const {
			QUARK_ASSERT(check_invariant());

			return _pod._int;
		}
		private: inline explicit bc_value_t(int value) :
#if DEBUG
			_debug_type(typeid_t::make_int()),
#endif
			_is_ext(false)
		{
			_pod._int = value;
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		float


		public: inline  static bc_value_t make_float(float v){
			return bc_value_t{ v };
		}
		public: float get_float_value() const {
			QUARK_ASSERT(check_invariant());

			return _pod._float;
		}
		private: inline  explicit bc_value_t(float value) :
#if DEBUG
			_debug_type(typeid_t::make_float()),
#endif
			_is_ext(false)
		{
			_pod._float = value;
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		string


		public: inline  static bc_value_t make_string(const std::string& v){
			return bc_value_t{ v };
		}
		public: inline  std::string get_string_value() const{
			QUARK_ASSERT(check_invariant());

			return _pod._ext->_string;
		}
		private: inline  explicit bc_value_t(const std::string value) :
#if DEBUG
			_debug_type(typeid_t::make_string()),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{value};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		json_value


		public: inline  static bc_value_t make_json_value(const json_t& v){
			return bc_value_t{ std::make_shared<json_t>(v) };
		}
		public: inline  json_t get_json_value() const{
			QUARK_ASSERT(check_invariant());

			return *_pod._ext->_json_value.get();
		}
		private: inline  explicit bc_value_t(const std::shared_ptr<json_t>& value) :
#if DEBUG
			_debug_type(typeid_t::make_json_value()),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{value};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		typeid


		public: inline  static bc_value_t make_typeid_value(const typeid_t& type_id){
			return bc_value_t{ type_id };
		}
		public: inline  typeid_t get_typeid_value() const {
			QUARK_ASSERT(check_invariant());

			return _pod._ext->_typeid_value;
		}
		private: inline  explicit bc_value_t(const typeid_t& type_id) :
#if DEBUG
			_debug_type(typeid_t::make_typeid()),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{type_id};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		struct


		public: inline  static bc_value_t make_struct_value(const typeid_t& struct_type, const std::vector<bc_value_t>& values){
			return bc_value_t{ struct_type, values, true };
		}
		public: inline  const std::vector<bc_value_t>& get_struct_value() const {
			return _pod._ext->_struct_members;
		}
		private: inline  explicit bc_value_t(const typeid_t& struct_type, const std::vector<bc_value_t>& values, bool struct_tag) :
#if DEBUG
			_debug_type(struct_type),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{struct_type, values, true};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		vector

		//??? remove all use of typeid_t -- clients will allocate them even if we don't keep the instances!
		public: inline static bc_value_t make_vector_value(const typeid_t& element_type, const std::vector<bc_value_t>& elements){
			return bc_value_t{element_type, elements};
		}
		public: inline const std::vector<bc_value_t>* get_vector_value() const{
			QUARK_ASSERT(check_invariant());

			return &_pod._ext->_vector_elements;
		}
		private: inline explicit bc_value_t(const typeid_t& element_type, const std::vector<bc_value_t>& values) :
#if DEBUG
			_debug_type(typeid_t::make_vector(element_type)),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{typeid_t::make_vector(element_type), values};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		dict

		//??? remove all make-functions -- just use constructors.
		public: inline static bc_value_t make_dict_value(const typeid_t& value_type, const std::map<std::string, bc_value_t>& entries){
			return bc_value_t{value_type, entries};
		}

		public: inline const std::map<std::string, bc_value_t>& get_dict_value() const{
			QUARK_ASSERT(check_invariant());

			return _pod._ext->_dict_entries;
		}
		private: inline explicit bc_value_t(const typeid_t& value_type, const std::map<std::string, bc_value_t>& entries) :
#if DEBUG
			_debug_type(typeid_t::make_dict(value_type)),
#endif
			_is_ext(true)
		{
			_pod._ext = new bc_value_object_t{typeid_t::make_dict(value_type), entries};
			QUARK_ASSERT(check_invariant());
		}


		//////////////////////////////////////		function


		public: inline static bc_value_t make_function_value(const typeid_t& function_type, int function_id){
			return bc_value_t{ function_type, function_id, true };
		}
		public: inline int get_function_value() const{
			QUARK_ASSERT(check_invariant());

			return _pod._function_id;
		}
		private: inline explicit bc_value_t(const typeid_t& function_type, int function_id, bool dummy) :
#if DEBUG
			_debug_type(function_type),
#endif
			_is_ext(false)
		{
			_pod._function_id = function_id;
			QUARK_ASSERT(check_invariant());
		}


		friend bc_value_t value_to_bc(const value_t& value);


		//	Bumps RC.
		public: inline static bc_value_t make_object(bc_value_object_t* ext){
			bc_value_t temp;
			temp._is_ext = true;
			ext->_rc++;
			temp._pod._ext = ext;
			return temp;
		}

		//	YES bump.
#if DEBUG
		public: inline explicit bc_value_t(typeid_t debug_type, const bc_pod_value_t& internals, bool is_ext) :
			_debug_type(debug_type),
			_pod(internals),
			_is_ext(is_ext)
		{
			if(_is_ext){
				_pod._ext->_rc++;
			}
		}
#else
		public: inline explicit bc_value_t(const bc_pod_value_t& internals, bool is_ext) :
			_pod(internals),
			_is_ext(is_ext)
		{
			if(_is_ext){
				_pod._ext->_rc++;
			}
		}
#endif


		//////////////////////////////////////		STATE
#if DEBUG
		public: typeid_t _debug_type;
#endif
		private: bool _is_ext;
		public: bc_pod_value_t _pod;
	};


	//////////////////////////////////////		COMPARE


	int bc_compare_value_true_deep(const bc_value_t& left, const bc_value_t& right, const typeid_t& type);


	//////////////////////////////////////		bc_opcode


	enum class bc_opcode: uint8_t {
		k_nop = 0,

		/*
			TYPE: ---
			A: Register: where to put result
			B: IMMEDIATE: global index
			C: ---
		*/
		k_load_global_obj,
		/*
			TYPE: ---
			A: Register: where to put result
			B: IMMEDIATE: global index
			C: ---
		*/
		k_load_global_intern,




		/*
			TYPE: ---
			A: IMMEDIATE: global index
			B: Register: source reg
			C: ---
		*/
		k_store_global_obj,

		/*
			TYPE: ---
			A: IMMEDIATE: global index
			B: Register: source reg
			C: ---
		*/
		k_store_global_intern,



		/*
			TYPE: ---
			A: Register: where to put result
			B: Register: parent
			C: ---
		*/
		k_store_local_intern,
		/*
			TYPE: ---
			A: Register: where to put result
			B: Register: parent
			C: ---
		*/
		k_store_local_obj,


		/*
			TYPE: ---
			A: Register: where to put result
			B: Register: struct object
			C: IMMEDIATE: member-index
		*/
		k_get_struct_member,


		/*
			TYPE: ---
			A: Register: where to put result
			B: Register: string object/vector object/json_value/dict
			C: Register: index (int)
		*/
		k_lookup_element_string,
		k_lookup_element_json_value,
		k_lookup_element_vector,
		k_lookup_element_dict,

		/*
			TYPE: itype of object
			A: Register: where to put result: integer
			B: Register: object
			C: ---
		*/
//		k_size,

		/*
			TYPE: itype of function output
			A: Register: tells where to put function return
			B: Register: function value to call
			C: IMMEDIATE: argument count. Values are put on stack. Notice that DYN arguments pushes itype first.

			All arguments are pushed to stack, first argument first.
			DYN arguments are pushed as (itype, value)
		*/
		k_call,

		/*
			TYPE: ---
			A: Register: where to put result
			B: Register: lhs
			C: Register: rhs
		*/
		k_add_bool,
		k_add_int,
		k_add_float,
		k_add_string,
		k_add_vector,
		k_subtract_float,
		k_subtract_int,
		k_multiply_float,
		k_multiply_int,
		k_divide_float,
		k_divide_int,
		k_remainder,
		k_remainder_int,

		k_logical_and_bool,
		k_logical_and_int,
		k_logical_and_float,
		k_logical_or_bool,
		k_logical_or_int,
		k_logical_or_float,


		//////////////////////////////////////		COMPARISON

		//??? Remove all conditions. Only have conditional branches.
		//??? Remove all >= -- just swap registers and use <.
		/*
			The type-unspecific version is a fallback to handles all types not special-cased.
			TYPE: ----
			A: Register: where to put result BOOL
			B: Register: lhs
			C: Register: rhs

			TYPE: ---
			A: Register: where to put result BOOL
			B: Register: lhs
			C: Register: rhs
		*/
		k_comparison_smaller_or_equal,
		k_comparison_smaller_or_equal_int,
		k_comparison_smaller,
		k_comparison_smaller_int,

		k_logical_equal,
		k_logical_equal_int,
		k_logical_nonequal,
		k_logical_nonequal_int,


		/*
			TYPE: ---
			A: Register: where to put resulting value
			B: IMMEDIATE: Target type - type to create.
			C: IMMEDIATE: Source itype

			Stack hold ONE value, which is the input value
		*/
		k_new_1,

		/*
			TYPE: ---
			A: Register: where to put resulting value
			B: IMMEDIATE: itype T = [E], describing output type of vector, like [int] or [my_pixel].
			C: IMMEDIATE: Argument count.

			Arguments are put on stack. No DYN arguments. All arguments are of type E.
		*/
		k_new_vector,

		/*
			TYPE: ---
			A: Register: where to put resulting value
			B: IMMEDIATE: itype T = [string:V], describing output type of dict, like [string:int] or [string:my_pixel].
			C: IMMEDIATE: Argument count. Each dict entry becomes TWO arguments: (string, V).

			Arguments are put on stack. No DYN arguments. All arguments are of type (string, V).

			Example:
				a = { "chewie": 10.0, "leia": 20.0, "luke": 30.0 }

				Gives:

				B: itype with [string, float]
				C: 6
				Stack: "chewie", 10.0, "leia", 20.0, "luke", 30.0
		*/
		k_new_dict,

		/*
			TYPE: ---
			A: Register: where to put resulting value
			B: IMMEDIATE: itype T of the struct
			C: IMMEDIATE: Argument count.

			Arguments are put on stack. No DYN arguments.
			All arguments are of types matching the struct members.
		*/
		k_new_struct,

		/*
			TYPE: ---.
			A: Register: value to return
			B: ---
			C: ---
		*/
		k_return,

		/*
			TYPE: ---
			A: ---
			B: ---
			C: ---
		*/
		k_stop,


		//////////////////////////////////////		STACK

		/*
			Pushes the VM frame info to stack.
			TYPE: ---
			A: ---
			B: ---
			C: ---
			STACK 1: a b c
			STACK 2: a b c [prev frame pos] [frame_ptr]
		*/
		k_push_frame_ptr,

		/*
			Pops the VM frame info to stack -- this changes the VM's frame pos & frame ptr.
			TYPE: ---
			A: ---
			B: ---
			C: ---
			STACK 1: a b c [prev frame pos] [frame_ptr]
			STACK 2: a b c
		*/
		k_pop_frame_ptr,


		///??? Could optimize by pushing 3 values with ONE instruction -- use A B C.
		///??? Could optimize by using a byte-stack and only pushing minimal number of bytes. Bool needs 1 byte only.

		/*
			TYPE: ---
			A: Register: where to read V
			B: ---
			C: ---
			STACK 1: a b c
			STACK 2: a b c V
		*/
		k_push_intern,

		/*
			NOTICE: This function bumps the RC of the pushed V-object. This represents the stack-entry co-owning V.
			TYPE: ---
			A: Register: where to read V
			B: ---
			C: ---
			STACK 1: a b c
			STACK 2: a b c V
		*/
		k_push_obj,

		/*
			TYPE: ---
			A: IMMEDIATE: arg count. 0 to 32.
			B: IMMEDIATE: extbits. bit 0 maps to the next value to be popped from stack.
			C: ---

			Example 1: one OBJ
			n:	1		 -------- -------- -------- -------=
			extbits:	%00000000 00000000 00000000 00000001
			STACK 1: a b c OBJ
			STACK 2: a b c

			Example 2: Three values.
			n:	4		 -------- -------- -------- ----====
			extbits:	%00000000 00000000 00000000 00001110
			STACK 1: a b c OBJ OBJ OBJ INTERN
			STACK 2: a b c
		*/
		k_popn,


		//////////////////////////////////////		BRANCH


		/*
			TYPE: ---
			A: Register: input to test
			B: IMMEDIATE: branch offset (added to PC) on branch.
			C: ---
			NOTICE: These have their own conditions, instead of check bool from k_comparison_smaller. Bad???
		*/
		k_branch_false_bool,
		k_branch_true_bool,
		k_branch_zero_int,
		k_branch_notzero_int,


		/*
			A: Register: lhs
			B: Register: rhs
			C: IMMEDIATE: branch offset (added to PC) on branch.
		*/
		k_branch_smaller_int,
		k_branch_smaller_or_equal_int,

		/*
			TYPE: ---
			A: ---
			B: IMMEDIATE: branch offset (added to PC) on branch.
			C: ---
		*/
		k_branch_always
	};


	struct opcode_info_t {
		std::string _as_text;
		enum class encoding {
			k_e_0000,
			k_f_trr0,
			k_g_trri,
			k_h_trrr,
			k_i_trii,
			k_j_tr00,
			k_k_0ri0,
			k_l_00i0,
			k_m_tr00,
			k_n_0ii0,
			k_o_0rrr,
			k_p_0r00,
			k_q_0rr0,
			k_r_0ir0,
			k_s_0rri,
			k_t_0rii
		};
		encoding _encoding;
	};

	extern const std::map<bc_opcode, opcode_info_t> k_opcode_info;



	//////////////////////////////////////		bc_instruction2_t


	/*
		??? IDEA for encoding:
		----------------------------------- -----------------------------------
		66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000
		32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210

		XXXXXXXX XXXXXXXX PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPppp
		48bit Intel x86_64 pointer. ppp = low bits, set to 0, X = bit 47

		-----------------------------------
		33222222 22221111 11111100 00000000
		10987654 32109876 54321098 76543210

		INSTRUCTION
		CCCCCCCC AAAAAAAA BBBBBBBB CCCCCCCC

		A = destination register.
		B = lhs register
		C = rhs register

		-----------------------------------------------------------------------
	*/

	struct bc_instruction2_t {
		bc_instruction2_t(
			bc_opcode opcode,
			int16_t a,
			int16_t b,
			int16_t c
		) :
			_opcode(opcode),
			_a(a),
			_b(b),
			_c(c)
		{
			QUARK_ASSERT(check_invariant());
		}

#if DEBUG
		public: bool check_invariant() const;
#endif


		//////////////////////////////////////		STATE
		bc_opcode _opcode;
		uint8_t _pad8;
		int16_t _a;
		int16_t _b;
		int16_t _c;
	};



	struct reg_flags_t {
		bool _type;
		bool _a;
		bool _b;
		bool _c;
	};
	reg_flags_t encoding_to_reg_flags(opcode_info_t::encoding e);




	//////////////////////////////////////		bc_frame_t


	struct bc_frame_t {
		bc_frame_t(const std::vector<bc_instruction2_t>& instrs2, const std::vector<std::pair<std::string, symbol_t>>& symbols, const std::vector<typeid_t>& args);
		public: bool check_invariant() const;


		//////////////////////////////////////		STATE

		std::vector<bc_instruction2_t> _instrs2;
		std::vector<std::pair<std::string, symbol_t>> _symbols;
		std::vector<typeid_t> _args;

		//	True if equivalent symbol is an ext.
		std::vector<bool> _exts;

		//	This doesn't count arguments.
		std::vector<bool> _locals_exts;
		std::vector<bc_value_t> _locals;
	};



	//////////////////////////////////////		bc_function_definition_t


	//???	All functions should be function values: host-functions and Floyd functions. This means _host_function_id should be in the VALUE not function definition!
	struct bc_function_definition_t {
		bc_function_definition_t(
			const typeid_t& function_type,
			const std::vector<member_t>& args,
			const std::shared_ptr<bc_frame_t>& frame,
			int host_function_id
		):
			_function_type(function_type),
			_args(args),
			_frame_ptr(frame),
			_host_function_id(host_function_id),
			_dyn_arg_count(-1),
			_return_is_ext(bc_value_t::is_bc_ext(_function_type.get_function_return().get_base_type()))
		{
			_dyn_arg_count = count_function_dynamic_args(function_type);
		}


#if DEBUG
		public: bool check_invariant() const {
			return true;
		}
#endif

		//////////////////////////////////////		STATE

		//??? store more optimzation stuff here!
		typeid_t _function_type;
		std::vector<member_t> _args;
		std::shared_ptr<bc_frame_t> _frame_ptr;
		int _host_function_id;

		int _dyn_arg_count;
		bool _return_is_ext;
	};


	//////////////////////////////////////		bc_program_t


	struct bc_program_t {
#if DEBUG
		public: bool check_invariant() const {
//			QUARK_ASSERT(_globals.check_invariant());
			return true;
		}
#endif

		public: const bc_frame_t _globals;
		public: std::vector<const bc_function_definition_t> _function_defs;
		public: std::vector<const typeid_t> _types;
	};

	json_t bcprogram_to_json(const bc_program_t& program);




	//////////////////////////////////////		frame_pos_t


	struct frame_pos_t {
		int _frame_pos;
		const bc_frame_t* _frame_ptr;
	};

	inline bool operator==(const frame_pos_t& lhs, const frame_pos_t& rhs){
		return lhs._frame_pos == rhs._frame_pos && lhs._frame_ptr == rhs._frame_ptr;
	}



	//////////////////////////////////////		interpreter_stack_t

	enum {
		//	We store prev-frame-pos & symbol-ptr.
		k_frame_overhead = 2
	};


	/*
		0	[int = 0] 		previous stack frame pos, 0 = global
		1	[symbols_ptr frame #0]
		2	[local0]		<- stack frame #0
		3	[local1]
		4	[local2]

		5	[int = 1] //	prev stack frame pos
		1	[symbols_ptr frame #1]
		7	[local1]		<- stack frame #1
		8	[local2]
		9	[local3]
	*/

	struct interpreter_stack_t {
		public: interpreter_stack_t(const bc_frame_t* global_frame) :
			_current_frame_pos(0),
			_current_frame_ptr(nullptr),
			_current_frame_entry_ptr(nullptr),
			_global_frame(global_frame),
			_entries(nullptr),
			_allocated_count(0),
			_stack_size(0)
		{
			_entries = new bc_pod_value_t[8192];
			_allocated_count = 8192;
			_current_frame_entry_ptr = &_entries[_current_frame_pos];

			QUARK_ASSERT(check_invariant());
		}

		public: ~interpreter_stack_t(){
			QUARK_ASSERT(check_invariant());

			delete[] _entries;
			_entries = nullptr;
		}

		public: bool check_invariant() const {
			QUARK_ASSERT(_entries != nullptr);
			QUARK_ASSERT(_stack_size >= 0 && _stack_size <= _allocated_count);
			QUARK_ASSERT(_current_frame_pos >= 0);
			QUARK_ASSERT(_current_frame_pos <= _stack_size);

			QUARK_ASSERT(_current_frame_entry_ptr == &_entries[_current_frame_pos]);

			QUARK_ASSERT(_debug_types.size() == _stack_size);
			for(int i = 0 ; i < _stack_size ; i++){
				QUARK_ASSERT(_debug_types[i].check_invariant());
			}
//			QUARK_ASSERT(_global_frame != nullptr);
			return true;
		}

		public: interpreter_stack_t(const interpreter_stack_t& other) = delete;

		public: interpreter_stack_t& operator=(const interpreter_stack_t& other) = delete;

/*
		public: interpreter_stack_t& operator=(const interpreter_stack_t& other){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(other.check_invariant());

			interpreter_stack_t temp = other;
			temp.swap(*this);
			return *this;
		}
*/

		public: void swap(interpreter_stack_t& other) throw() {
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(other.check_invariant());

			std::swap(other._entries, _entries);
			std::swap(other._allocated_count, _allocated_count);
			std::swap(other._stack_size, _stack_size);
#if DEBUG
			other._debug_types.swap(_debug_types);
#endif
			std::swap(_current_frame_pos, other._current_frame_pos);
			std::swap(_current_frame_ptr, other._current_frame_ptr);
			std::swap(_current_frame_entry_ptr, other._current_frame_entry_ptr);

			std::swap(_global_frame, other._global_frame);

			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(other.check_invariant());
		}

		public: inline int size() const {
			QUARK_ASSERT(check_invariant());

			return static_cast<int>(_stack_size);
		}


		public: bool check_global_access_obj(const int global_index) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(global_index >= 0 && global_index < (k_frame_overhead + _global_frame->_symbols.size()));
			QUARK_ASSERT(_global_frame->_exts[global_index] == true);
			return true;
		}
		public: bool check_global_access_intern(const int global_index) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(global_index >= 0 && global_index < (k_frame_overhead + _global_frame->_symbols.size()));
			QUARK_ASSERT(_global_frame->_exts[global_index] == false);
			return true;
		}


		//////////////////////////////////////		FRAME


		public: int read_prev_frame_pos(int frame_pos) const;
		public: frame_pos_t read_prev_frame_pos2(int frame_pos) const;

		public: const bc_frame_t* read_prev_frame(int frame_pos) const;
#if DEBUG
		public: bool check_stack_frame(int frame_pos, const bc_frame_t* frame) const;
#endif

		//	??? DYN values /returns needs TWO registers.
		//	??? This function should just allocate a block for frame, then have a list of writes. ALTERNATIVELY: generatet instructions to do this in the VM?
		//	Returns new frame-pos, same as vm._current_stack_frame.
		public: void open_frame(const bc_frame_t& frame, int values_already_on_stack){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(frame.check_invariant());
			QUARK_ASSERT(values_already_on_stack == frame._args.size());

			const auto stack_end = size();
			const auto parameter_count = static_cast<int>(frame._args.size());

			//	Carefully position the new stack frame so its includes the parameters that already sits in the stack.
			//	The stack frame already has symbols/registers mapped for those parameters.
			const auto new_frame_pos = stack_end - parameter_count;

			for(int i = 0 ; i < frame._locals.size() ; i++){
				bool ext = frame._locals_exts[i];
				const auto& local = frame._locals[i];
				if(ext){
					push_obj(local);
				}
				else{
					push_intern(local);
				}
			}
			_current_frame_pos = new_frame_pos;
			_current_frame_ptr = &frame;
			_current_frame_entry_ptr = &_entries[_current_frame_pos];
		}


		//	Pops all locals, decrementing RC when needed.
		//	Decrements all stack frame object RCs.
		//	Caller handles RC for parameters, this function don't.
		public: void close_frame(const bc_frame_t& frame){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(frame.check_invariant());

			//	Using symbol table to figure out which stack-frame values needs RC. Decrement them all.
			pop_batch(frame._locals_exts);
		}

		public: std::vector<std::pair<int, int>> get_stack_frames(int frame_pos) const;

		public: inline frame_pos_t find_frame_pos(int parent_step) const{
			QUARK_ASSERT(check_invariant());

			QUARK_ASSERT(parent_step == 0 || parent_step == -1);
			if(parent_step == 0){
				return { _current_frame_pos, _current_frame_ptr};
			}
			else if(parent_step == -1){
				return frame_pos_t{k_frame_overhead, _global_frame};
			}
			else{
				QUARK_ASSERT(false);
				throw std::exception();
			}
		}

		public: bool check_reg(int reg) const{
			QUARK_ASSERT(reg >= 0 && reg < _current_frame_ptr->_symbols.size());
			return true;
		}

		public: const std::pair<std::string, symbol_t>* get_register_info2(int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(reg >= 0 && reg < _current_frame_ptr->_symbols.size());

			return &_current_frame_ptr->_symbols[reg];
		}


		public: inline const bc_pod_value_t& peek_register(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));

			return _current_frame_entry_ptr[reg];
		}
		public: inline const void write_register_pod(const int reg, const bc_pod_value_t& pod) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));

			_current_frame_entry_ptr[reg] = pod;
		}

		public: bc_value_t read_register(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));

			QUARK_ASSERT(_debug_types[_current_frame_pos + reg] == _current_frame_ptr->_symbols[reg].second._value_type);

			bool is_ext = _current_frame_ptr->_exts[reg];
#if DEBUG
			const auto result = bc_value_t(_current_frame_ptr->_symbols[reg].second._value_type, _current_frame_entry_ptr[reg], is_ext);
#else
			const auto result = bc_value_t(_current_frame_entry_ptr[reg], is_ext);
#endif
			return result;
		}
		public: void write_register(const int reg, const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(value.check_invariant());

			QUARK_ASSERT(_debug_types[_current_frame_pos + reg] == _current_frame_ptr->_symbols[reg].second._value_type);
			bool is_ext = _current_frame_ptr->_exts[reg];
			if(is_ext){
				auto prev_copy = _current_frame_entry_ptr[reg];
				value._pod._ext->_rc++;
				_current_frame_entry_ptr[reg] = value._pod;
				bc_value_t::release_ext_pod(prev_copy);
			}
			else{
				_current_frame_entry_ptr[reg] = value._pod;
			}

			QUARK_ASSERT(check_invariant());
		}

/*
		public: bc_value_t read_register_intern(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));

			const auto info = get_register_info2(reg);
			QUARK_ASSERT(bc_value_t::is_bc_ext(info->second._value_type.get_base_type()) == false);

#if DEBUG
			return bc_value_t(_debug_types[_current_frame_pos + reg], _current_frame_entry_ptr[reg], false);
#else
			return bc_value_t(_current_frame_entry_ptr[reg], false);
#endif
		}

		public: void write_register_intern(const int reg, const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(value.check_invariant());
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == value._debug_type);
		#endif

			_current_frame_entry_ptr[reg] = value._pod;
		}

		//??? use const bc_value_object_t* peek_register_obj()
		public: bc_value_t read_register_obj(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(bc_value_t::is_bc_ext(info->second._value_type.get_base_type()) == true);
		#endif

#if DEBUG
			return bc_value_t(_debug_types[_current_frame_pos + reg], _current_frame_entry_ptr[reg], true);
#else
			return bc_value_t(_current_frame_entry_ptr[reg], true);
#endif
		}
*/
		public: void write_register_obj(const int reg, const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(value.check_invariant());
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == value._debug_type);
		#endif
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg] == value._debug_type);

			auto prev_copy = _current_frame_entry_ptr[reg];
			value._pod._ext->_rc++;
			_current_frame_entry_ptr[reg] = value._pod;
			bc_value_t::release_ext_pod(prev_copy);

			QUARK_ASSERT(check_invariant());
		}

/*
		public: bool read_register_bool(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == typeid_t::make_bool());
		#endif

			return _current_frame_entry_ptr[reg]._bool;
		}
		public: void write_register_bool(const int reg, bool value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == typeid_t::make_bool());
		#endif

			_current_frame_entry_ptr[reg]._bool = value;
		}
*/

		#if DEBUG
		public: bool check_reg_any(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_bool(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_bool());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_int(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_int());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_float(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_float());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_string(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_string());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_json(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_json_value());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_vector(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_vector());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_dict(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_dict());
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_struct(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_struct());
			return true;
		}
		#endif


		#if DEBUG
		public: bool check_reg_obj(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_current_frame_ptr->_exts[reg] == true);
			return true;
		}
		#endif

		#if DEBUG
		public: bool check_reg_intern(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_current_frame_ptr->_exts[reg] == false);
			return true;
		}
		#endif

/*
		public: int read_register_int_xxx(const int reg) const{
			QUARK_ASSERT(check_register_int_access(reg));

			return _current_frame_entry_ptr[reg]._int;
		}
		public: void write_register_int_xxx(const int reg, int value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == typeid_t::make_int());
		#endif

			_current_frame_entry_ptr[reg]._int = value;
		}

		public: void write_register_float(const int reg, float value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == typeid_t::make_float());
		#endif

			_current_frame_entry_ptr[reg]._float = value;
		}

		public: const std::string& peek_register_string(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type == typeid_t::make_string());
		#endif

			return _current_frame_entry_ptr[reg]._ext->_string;
		}
*/

		public: void write_register_string(const int reg, const std::string& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
			QUARK_ASSERT(_debug_types[_current_frame_pos + reg].is_string());

			const auto value2 = bc_value_t::make_string(value);

			auto prev_copy = _current_frame_entry_ptr[reg];
			value2._pod._ext->_rc++;
			_current_frame_entry_ptr[reg] = value2._pod;
			bc_value_t::release_ext_pod(prev_copy);

			QUARK_ASSERT(check_invariant());
		}

		public: inline int read_register_function(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type.get_base_type() == base_type::k_function);
		#endif

			return _current_frame_entry_ptr[reg]._function_id;
		}

/*
		public: const std::vector<bc_value_t>* peek_register_vector(const int reg) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(check_reg(reg));
		#if DEBUG
			const auto info = get_register_info2(reg);
			QUARK_ASSERT(info->second._value_type.get_base_type() == base_type::k_vector);
		#endif

			return &_current_frame_entry_ptr[reg]._ext->_vector_elements;
		}
*/






		//////////////////////////////////////		STACK




		public: void save_frame(){
			const auto frame_pos = bc_value_t::make_int(_current_frame_pos);
			push_intern(frame_pos);

			const auto frame_ptr = bc_value_t(_current_frame_ptr);
			push_intern(frame_ptr);
		}
		public: void restore_frame(){
			const auto stack_size = size();
			bc_pod_value_t frame_ptr_pod = load_pod(stack_size - 1);
			bc_pod_value_t frame_pos_pod = load_pod(stack_size - 2);

			_current_frame_ptr = frame_ptr_pod._frame_ptr;
			_current_frame_pos = frame_pos_pod._int;
			_current_frame_entry_ptr = &_entries[_current_frame_pos];

			pop(false);
			pop(false);
		}



		//////////////////////////////////////		STACK


		public: inline void push_value(const bc_value_t& value, bool is_ext){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(value.check_invariant());
#if DEBUG
			QUARK_ASSERT(bc_value_t::is_bc_ext(value._debug_type.get_base_type()) == is_ext);
#endif

			if(is_ext){
				value._pod._ext->_rc++;
			}
			_entries[_stack_size] = value._pod;
			_stack_size++;
#if DEBUG
			_debug_types.push_back(value._debug_type);
#endif

			QUARK_ASSERT(check_invariant());
		}
/*
		private: inline void push_intq(int value){
			QUARK_ASSERT(check_invariant());

			//??? Can write directly into entry, no need to construct e.
			bc_pod_value_t e;
			e._int = value;
			_entries[_stack_size] = e;
			_stack_size++;
#if DEBUG
			_debug_types.push_back(typeid_t::make_int());
#endif

			QUARK_ASSERT(check_invariant());
		}
*/
		private: inline void push_obj(const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(value.check_invariant());
#if DEBUG
			QUARK_ASSERT(bc_value_t::is_bc_ext(value._debug_type.get_base_type()) == true);
#endif

			value._pod._ext->_rc++;
			_entries[_stack_size] = value._pod;
			_stack_size++;
#if DEBUG
			_debug_types.push_back(value._debug_type);
#endif

			QUARK_ASSERT(check_invariant());
		}
		public: inline void push_intern(const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(value.check_invariant());
#if DEBUG
			QUARK_ASSERT(bc_value_t::is_bc_ext(value._debug_type.get_base_type()) == false);
#endif

			_entries[_stack_size] = value._pod;
			_stack_size++;
#if DEBUG
			_debug_types.push_back(value._debug_type);
#endif

			QUARK_ASSERT(check_invariant());
		}


		//	returned value will have ownership of obj, if it's an obj.
		public: inline bc_value_t load_value_slow(int pos, const typeid_t& type) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(type.check_invariant());
			QUARK_ASSERT(type == _debug_types[pos]);

			const auto& e = _entries[pos];
			bool is_ext = bc_value_t::is_bc_ext(type.get_base_type());

#if DEBUG
			const auto result = bc_value_t(type, e, is_ext);
#else
			const auto result = bc_value_t(e, is_ext);
#endif
			return result;
		}

		public: inline const bc_pod_value_t& load_pod(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			return _entries[pos];
		}
		public: inline bc_value_t load_intern_value(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(bc_value_t::is_bc_ext(_debug_types[pos].get_base_type()) == false);

#if DEBUG
			const auto result = bc_value_t(_debug_types[pos], _entries[pos], false);
#else
			const auto result = bc_value_t(_entries[pos], false);
#endif
			return result;
		}
/*
		public: inline bc_value_t load_obj(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(bc_value_t::is_bc_ext(_debug_types[pos].get_base_type()) == true);

#if DEBUG
			const auto result = bc_value_t(_debug_types[pos], _entries[pos], true);
#else
			const auto result = bc_value_t(_entries[pos], true);
#endif
			return result;
		}
*/
		public: inline int load_intq(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(bc_value_t::is_bc_ext(_debug_types[pos].get_base_type()) == false);

			return _entries[pos]._int;
		}
		private: inline const bc_frame_t* load_frame_ptr(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(bc_value_t::is_bc_ext(_debug_types[pos].get_base_type()) == false);

			return _entries[pos]._frame_ptr;
		}
/*
		private: inline void push_frame_ptr(const bc_frame_t* frame_ptr){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(frame_ptr != nullptr && frame_ptr->check_invariant());

			const auto value = bc_value_t(frame_ptr);
			_entries[_stack_size] = value._pod;
			_stack_size++;
#if DEBUG
			_debug_types.push_back(value._debug_type);
#endif

		}
*/

		//??? We could have support simple sumtype called DYN that holds a value_t at runtime.


		public: inline void replace_intern(int pos, const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(value.check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
#if FLOYD_BC_VALUE_DEBUG_TYPE
			QUARK_ASSERT(bc_value_t::is_bc_ext(value._debug_type.get_base_type()) == false);
#endif
			QUARK_ASSERT(_debug_types[pos] == value._debug_type);

			_entries[pos] = value._pod;

			QUARK_ASSERT(check_invariant());
		}
/*
		public: inline void replace_pod(int pos, const bc_pod_value_t& pod){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);

			_entries[pos] = pod;

			QUARK_ASSERT(check_invariant());
		}
*/
		public: inline void replace_obj(int pos, const bc_value_t& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(value.check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
#if FLOYD_BC_VALUE_DEBUG_TYPE
			QUARK_ASSERT(bc_value_t::is_bc_ext(value._debug_type.get_base_type()) == true);
#endif
			QUARK_ASSERT(_debug_types[pos] == value._debug_type);

			auto prev_copy = _entries[pos];
			value._pod._ext->_rc++;
			_entries[pos] = value._pod;
			bc_value_t::release_ext_pod(prev_copy);

			QUARK_ASSERT(check_invariant());
		}
/*
		private: inline void replace_int(int pos, const int& value){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			QUARK_ASSERT(_debug_types[pos] == typeid_t::make_int());

			_entries[pos]._int = value;

			QUARK_ASSERT(check_invariant());
		}
*/

		//	extbits[0] tells if the first popped value has ext. etc.
		//	bit 0 maps to the next value to be popped from stack
		//	Max 32 can be popped.
		public: inline void pop_batch(int count, uint32_t extbits){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(_stack_size >= count);
			QUARK_ASSERT(count >= 0);
			QUARK_ASSERT(count <= 32);

			uint32_t bits = extbits;
			for(int i = 0 ; i < count ; i++){
				bool ext = (bits & 1) ? true : false;
				pop(ext);
				bits = bits >> 1;
			}
			QUARK_ASSERT(check_invariant());
		}

		//	exts[exts.size() - 1] maps to the closed value on stack, the next to be popped.
		public: inline void pop_batch(const std::vector<bool>& exts){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(_stack_size >= exts.size());

			auto flag_index = exts.size() - 1;
			for(int i = 0 ; i < exts.size() ; i++){
				pop(exts[flag_index]);
				flag_index--;
			}
			QUARK_ASSERT(check_invariant());
		}
		private: inline void pop(bool ext){
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(_stack_size > 0);
			QUARK_ASSERT(bc_value_t::is_bc_ext(_debug_types.back().get_base_type()) == ext);

			auto copy = _entries[_stack_size - 1];
			_stack_size--;
#if DEBUG
			_debug_types.pop_back();
#endif
			if(ext){
				bc_value_t::release_ext_pod(copy);
			}

			QUARK_ASSERT(check_invariant());
		}

#if DEBUG
		private: bool is_ext(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			return bc_value_t::is_bc_ext(_debug_types[pos].get_base_type());
		}
#endif

/*
#if DEBUG
		public: typeid_t debug_get_type(int pos) const{
			QUARK_ASSERT(check_invariant());
			QUARK_ASSERT(pos >= 0 && pos < _stack_size);
			return _debug_types[pos];
		}
#endif
*/

		public: frame_pos_t get_current_frame_pos() const {
			QUARK_ASSERT(check_invariant());

			return frame_pos_t{_current_frame_pos, _current_frame_ptr};
		}

		public: json_t stack_to_json() const;


		public: bc_pod_value_t* _entries;
		public: size_t _allocated_count;
		public: size_t _stack_size;

		//	These are DEEP copies = do not share RC with non-debug values.
#if DEBUG
		public: std::vector<typeid_t> _debug_types;
#endif

		public: int _current_frame_pos;
		public: const bc_frame_t* _current_frame_ptr;
		public: bc_pod_value_t* _current_frame_entry_ptr;

		public: const bc_frame_t* _global_frame;
	};




int get_global_n_pos(int n);





	//////////////////////////////////////		interpreter_imm_t


	struct interpreter_imm_t {
		////////////////////////		STATE
		public: const std::chrono::time_point<std::chrono::high_resolution_clock> _start_time;
		public: const bc_program_t _program;
		public: const std::map<int, HOST_FUNCTION_PTR> _host_functions;
	};


	//////////////////////////////////////		interpreter_t

	/*
		Complete runtime state of the interpreter.
		MUTABLE
	*/

	struct interpreter_t {
		public: explicit interpreter_t(const bc_program_t& program);
		public: interpreter_t(const interpreter_t& other) = delete;
		public: const interpreter_t& operator=(const interpreter_t& other)= delete;
#if DEBUG
		public: bool check_invariant() const;
#endif
		public: void swap(interpreter_t& other) throw();

		////////////////////////		STATE
		public: std::shared_ptr<interpreter_imm_t> _imm;



		//	Holds all values for all environments.
		//	Notice: stack holds refs to RC-counted objects!
		public: interpreter_stack_t _stack;
		public: std::vector<std::string> _print_output;
	};



	value_t call_host_function(interpreter_t& vm, int function_id, const std::vector<value_t>& args);
	value_t call_function(interpreter_t& vm, const value_t& f, const std::vector<value_t>& args);
	json_t interpreter_to_json(const interpreter_t& vm);
	std::pair<bool, bc_value_t> execute_instructions(interpreter_t& vm, const std::vector<bc_instruction2_t>& instructions);

} //	floyd


#endif /* bytecode_interpreter_hpp */