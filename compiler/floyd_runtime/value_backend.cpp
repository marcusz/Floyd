//
//  value_backend.cpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-18.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "value_backend.h"

#include <string>
#include <vector>

#include "types.h"
#include "json_support.h"
#include "compiler_basics.h"
#include "quark.h"
#include "format_table.h"

namespace floyd {

//#define QUARK_TEST QUARK_TEST_VIP


////////////////////////////////		heap_t


static uint64_t size_to_allocation_blocks(std::size_t size){
	const auto r = (size >> 3) + ((size & 7) > 0 ? 1 : 0);

	QUARK_ASSERT((r * sizeof(uint64_t) - size) >= 0);
	QUARK_ASSERT((r * sizeof(uint64_t) - size) < sizeof(uint64_t));

	return r;
}

static void trace_alloc(const heap_rec_t& e){
	QUARK_TRACE_SS(""


		<< std::to_string(e.alloc_ptr->alloc_id)
		<< "\t" << "magic: " << value_to_hex_string(e.alloc_ptr->magic, 8)
//		<< "used: " << e.in_use
		<< "\t" << "rc: " << e.alloc_ptr->rc

		<< "\t" << "debug_info: " << get_debug_info(*e.alloc_ptr)
		<< "\t" << "debug_value_type: " << e.alloc_ptr->debug_value_type.get_data()
		<< "\t" << "data[0]: " << e.alloc_ptr->data[0]
		<< "\t" << "data[1]: " << e.alloc_ptr->data[1]
		<< "\t" << "data[2]: " << e.alloc_ptr->data[2]
		<< "\t" << "data[3]: " << e.alloc_ptr->data[3]
	);
}

void trace_heap(const heap_t& heap){
	QUARK_ASSERT(heap.check_invariant());

	if(true){
		QUARK_SCOPED_TRACE("HEAP");

		if(heap.record_allocs_flag && k_heap_mutex){
			std::lock_guard<std::recursive_mutex> guard(*heap.alloc_records_mutex);
			for(int i = 0 ; i < heap.alloc_records.size() ; i++){
				const auto& e = heap.alloc_records[i];
				QUARK_ASSERT(e.alloc_ptr->check_invariant());
				trace_alloc(e);
			}
		}
	}
}

heap_t::heap_t(bool record_allocs_flag) :
	magic(HEAP_MAGIC),
	allocation_id_generator(k_alloc_start_id),
	record_allocs_flag(record_allocs_flag)
{
	if(k_heap_mutex){
		alloc_records_mutex = std::make_shared<std::recursive_mutex>();
	}
	QUARK_ASSERT(check_invariant());
}

heap_t::~heap_t(){
	QUARK_ASSERT(check_invariant());

#if DEBUG && 0
	const auto leaks = count_used();
	if(leaks > 0){
		QUARK_SCOPED_TRACE("LEAKS");
		trace_heap(*this);
	}
#endif
}

#if DEBUG
std::string get_debug_info(const heap_alloc_64_t& alloc){
	return alloc.debug_info;
}
#else
std::string get_debug_info(const heap_alloc_64_t& alloc){
	return "";
}
#endif

static inline void add_ref(heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	inc_rc(alloc);
}

static inline void release_ref(heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	if(dec_rc(alloc) == 0){
		dispose_alloc(alloc);
	}
}



static heap_alloc_64_t* alloc_64__internal(heap_t& heap, uint64_t allocation_word_count, type_t debug_value_type, const char debug_string[]){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(debug_string != nullptr);
	QUARK_ASSERT(debug_value_type.check_invariant());

	const auto header_size = sizeof(heap_alloc_64_t);
	QUARK_ASSERT((header_size % 8) == 0);

	const auto malloc_size = header_size + allocation_word_count * sizeof(uint64_t);
	void* alloc0 = std::malloc(malloc_size);
	if(alloc0 == nullptr){
		throw std::exception();
	}

	auto alloc = new (alloc0) heap_alloc_64_t(&heap, allocation_word_count, debug_value_type, debug_string);
	QUARK_ASSERT(alloc->rc == 1);
	QUARK_ASSERT(alloc->check_invariant());
	if(heap.record_allocs_flag){
		std::lock_guard<std::recursive_mutex> guard(*heap.alloc_records_mutex);
		heap.alloc_records.push_back({ alloc });
	}

	QUARK_ASSERT(alloc->check_invariant());
	QUARK_ASSERT(heap.check_invariant());
	return alloc;
}
heap_alloc_64_t* alloc_64(heap_t& heap, uint64_t allocation_word_count, type_t debug_value_type, const char debug_string[]){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(debug_string != nullptr);

	const auto header_size = sizeof(heap_alloc_64_t);
	QUARK_ASSERT((header_size % 8) == 0);

	if(k_heap_mutex){
		std::lock_guard<std::recursive_mutex> guard(*heap.alloc_records_mutex);
		return alloc_64__internal(heap, allocation_word_count, debug_value_type, debug_string);
	}
	else{
		return alloc_64__internal(heap, allocation_word_count, debug_value_type, debug_string);
	}
}


QUARK_TEST("heap_t", "alloc_64()", "", ""){
	heap_t heap(false);
	QUARK_VERIFY(heap.check_invariant());
}

/*
QUARK_TEST("heap_t", "alloc_64()", "", ""){
	heap_t heap(false);
	auto a = alloc_64(heap, 0, make_vector(), "test");
	QUARK_VERIFY(a != nullptr);
	QUARK_VERIFY(a->check_invariant());
	QUARK_VERIFY(a->rc == 1);
#if DEBUG
	QUARK_VERIFY(get_debug_info(*a) == "test");
#endif

	//	Must release alloc or heap will detect leakage.
	release_ref(*a);
}

QUARK_TEST("heap_t", "add_ref()", "", ""){
	heap_t heap(false);
	auto a = alloc_64(heap, 0, make_vector(), "test");
	add_ref(*a);
	QUARK_VERIFY(a->rc == 2);

	//	Must release alloc or heap will detect leakage.
	release_ref(*a);
}

QUARK_TEST("heap_t", "release_ref()", "", ""){
	heap_t heap(false);
	auto a = alloc_64(heap, 0, make_vector(), "test");

	QUARK_VERIFY(a->rc == 1);
	release_ref(*a);
	QUARK_VERIFY(a->rc == 0);

	const auto count = heap.count_used();
	QUARK_VERIFY(count == 0);
}
*/


//	Returns pointer to the allocated words that sits after the
void* get_alloc_ptr(heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	const auto size = sizeof(heap_alloc_64_t);
	const auto p0 = reinterpret_cast<uint8_t*>(&alloc);
	const auto p1 = p0 + size;
	auto p = &alloc;
	const auto result = p + 1;
	QUARK_ASSERT(reinterpret_cast<uint8_t*>(result) == p1);
	return result;
}
const void* get_alloc_ptr(const heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	auto p = &alloc;
	return p + 1;
}




bool heap_alloc_64_t::check_invariant() const{
	QUARK_ASSERT(debug_value_type.is_undefined() == false);

	if(magic == ALLOC_64_MAGIC){
		QUARK_ASSERT(magic == ALLOC_64_MAGIC);
		QUARK_ASSERT(heap != nullptr);
		assert(heap != nullptr);
		QUARK_ASSERT(heap->magic == HEAP_MAGIC);
		QUARK_ASSERT(this->alloc_id >= k_alloc_start_id && this->alloc_id < heap->allocation_id_generator);

	//	auto it = std::find_if(heap->alloc_records.begin(), heap->alloc_records.end(), [&](heap_rec_t& e){ return e.alloc_ptr == this; });
	//	QUARK_ASSERT(it != heap->alloc_records.end());
	}
	else{
		if(k_keep_deleted_allocs){
			QUARK_ASSERT(magic == ALLOC_64_MAGIC_DELETED);
			QUARK_ASSERT(heap != nullptr);
			assert(heap != nullptr);
			QUARK_ASSERT(heap->magic == HEAP_MAGIC);
			QUARK_ASSERT(this->alloc_id >= k_alloc_start_id && this->alloc_id < heap->allocation_id_generator);

		//	auto it = std::find_if(heap->alloc_records.begin(), heap->alloc_records.end(), [&](heap_rec_t& e){ return e.alloc_ptr == this; });
		//	QUARK_ASSERT(it != heap->alloc_records.end());
		}
		else{
			QUARK_ASSERT(false);
		}
	}
	return true;
}



static void dispose_alloc__internal(heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	if(alloc.heap->record_allocs_flag){
		auto it = std::find_if(
			alloc.heap->alloc_records.begin(),
			alloc.heap->alloc_records.end(),
			[&](heap_rec_t& e){ return e.alloc_ptr == &alloc; }
		);
		QUARK_ASSERT(it != alloc.heap->alloc_records.end());

//		QUARK_ASSERT(it->in_use);
//		it->in_use = false;
	}

	if(k_keep_deleted_allocs){
		alloc.magic = ALLOC_64_MAGIC_DELETED;
	}
	else{
		//??? we don't delete the malloc() block in debug version.
#if DEBUG
		alloc.magic = ALLOC_64_MAGIC;
		alloc.data[0] = 0xdeadbeef'00000001;
		alloc.data[1] = 0xdeadbeef'00000002;
		alloc.data[2] = 0xdeadbeef'00000003;
		alloc.data[3] = 0xdeadbeef'00000004;

		alloc.allocation_word_count = 0xdeadbeef'00000005;

		alloc.heap = reinterpret_cast<heap_t*>(0xdeadbeef'00000005);
		alloc.debug_info = "disposed alloc";
#endif
		std::free(&alloc);
	}
}
void dispose_alloc(heap_alloc_64_t& alloc){
	QUARK_ASSERT(alloc.check_invariant());

	if(k_heap_mutex){
		std::lock_guard<std::recursive_mutex> guard(*alloc.heap->alloc_records_mutex);
		dispose_alloc__internal(alloc);
	}
	else{
		dispose_alloc__internal(alloc);
	}
}


bool heap_t::check_invariant() const{
#if 0
	for(const auto& e: alloc_records){
		QUARK_ASSERT(e.alloc_ptr != nullptr);
		QUARK_ASSERT(e.alloc_ptr->heap == this);
		QUARK_ASSERT(e.alloc_ptr->check_invariant());

/*
		if(e.in_use){
			QUARK_ASSERT(e.alloc_ptr->rc > 0);
		}
		else{
			QUARK_ASSERT(e.alloc_ptr->rc == 0);
		}
*/

	}
#endif
	return true;
}

static int count_used__internal(const heap_t& heap){
	QUARK_ASSERT(heap.check_invariant());

	int result = 0;
	if(heap.record_allocs_flag){
		for(const auto& e: heap.alloc_records){
			if(e.alloc_ptr->rc > 0){
				result = result + 1;
			}
		}
		return result;
	}
	else{
		return 0;
	}
}
int heap_t::count_used() const {
	QUARK_ASSERT(check_invariant());

	if(k_heap_mutex){
		std::lock_guard<std::recursive_mutex> guard(*alloc_records_mutex);
		return count_used__internal(*this);
	}
	else{
		return count_used__internal(*this);
	}
}




////////////////////////////////	runtime_type_t



runtime_type_t make_runtime_type(type_t type){
	return type.get_data();
}



////////////////////////////////	runtime_value_t


QUARK_TEST("", "", "", ""){
	const auto s = sizeof(runtime_value_t);
	QUARK_VERIFY(s == 8);
}



QUARK_TEST("", "", "", ""){
	auto y = make_runtime_int(8);
	auto a = make_runtime_int(1234);

	y = a;

	runtime_value_t c(y);
	(void)c;
}





runtime_value_t make_blank_runtime_value(){
	return make_runtime_int(UNINITIALIZED_RUNTIME_VALUE);
}

runtime_value_t make_runtime_bool(bool value){
	return { .bool_value = (uint8_t)(value ? 1 : 0) };
}

runtime_value_t make_runtime_int(int64_t value){
	return { .int_value = value };
}

runtime_value_t make_runtime_double(double value){
	return { .double_value = value };
}

runtime_value_t make_runtime_typeid(type_t type){
	return { .typeid_itype = type.get_data() };
}

runtime_value_t make_runtime_struct(STRUCT_T* struct_ptr){
	return { .struct_ptr = struct_ptr };
}

runtime_value_t make_runtime_vector_carray(VECTOR_CARRAY_T* vector_ptr){
	return { .vector_carray_ptr = vector_ptr };
}
runtime_value_t make_runtime_vector_hamt(VECTOR_HAMT_T* vector_hamt_ptr){
	return { .vector_hamt_ptr = vector_hamt_ptr };
}

runtime_value_t make_runtime_dict_cppmap(DICT_CPPMAP_T* dict_cppmap_ptr){
	return { .dict_cppmap_ptr = dict_cppmap_ptr };
}

runtime_value_t make_runtime_dict_hamt(DICT_HAMT_T* dict_hamt_ptr){
	return { .dict_hamt_ptr = dict_hamt_ptr };
}







uint64_t get_vec_string_size(runtime_value_t str){
	QUARK_ASSERT(str.vector_carray_ptr != nullptr);

	return str.vector_carray_ptr->get_element_count();
}

void copy_elements(runtime_value_t dest[], runtime_value_t source[], uint64_t count){
	for(auto i = 0 ; i < count ; i++){
		dest[i] = source[i];
	}
}






////////////////////////////////		VECTOR_CARRAY_T



QUARK_TEST("", "", "", ""){
	const auto vec_struct_size = sizeof(std::vector<int>);
	QUARK_VERIFY(vec_struct_size == 24);
}



VECTOR_CARRAY_T::~VECTOR_CARRAY_T(){
	QUARK_ASSERT(check_invariant());
}

bool VECTOR_CARRAY_T::check_invariant() const {
	QUARK_ASSERT(this->alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_vector() || alloc.debug_value_type.is_string());
	QUARK_ASSERT(get_debug_info(alloc) == "cppvec");
	return true;
}

runtime_value_t alloc_vector_carray(heap_t& heap, uint64_t allocation_count, uint64_t element_count, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(value_type.is_vector() || value_type.is_string());

	heap_alloc_64_t* alloc = alloc_64(heap, allocation_count, value_type, "cppvec");
	alloc->data[0] = element_count;

	auto vec = reinterpret_cast<VECTOR_CARRAY_T*>(alloc);

	QUARK_ASSERT(vec->check_invariant());
	QUARK_ASSERT(heap.check_invariant());

	return { .vector_carray_ptr = vec };
}


void dispose_vector_carray(const runtime_value_t& value){
	QUARK_ASSERT(sizeof(VECTOR_CARRAY_T) == sizeof(heap_alloc_64_t));

	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(value.vector_carray_ptr != nullptr);
	QUARK_ASSERT(value.vector_carray_ptr->check_invariant());

	auto heap = value.vector_carray_ptr->alloc.heap;
	dispose_alloc(value.vector_carray_ptr->alloc);
	QUARK_ASSERT(heap->check_invariant());
}







QUARK_TEST("VECTOR_CARRAY_T", "", "", ""){
	const auto vec_struct_size1 = sizeof(std::vector<int>);
	QUARK_VERIFY(vec_struct_size1 == 24);
}

/*
QUARK_TEST("VECTOR_CARRAY_T", "", "", ""){
	heap_t heap(false);
	detect_leaks(heap);

	auto v = alloc_vector_carray(heap, 3, 3, make_vector());
	QUARK_VERIFY(v.vector_carray_ptr != nullptr);

	if(dec_rc(v.vector_carray_ptr->alloc) == 0){
		dispose_vector_carray(v);
	}

	QUARK_VERIFY(heap.check_invariant());
	detect_leaks(heap);
}
*/




////////////////////////////////		VECTOR_HAMT_T



QUARK_TEST("", "", "", ""){
	const auto vec_size = sizeof(immer::vector<runtime_value_t>);
	QUARK_VERIFY(vec_size == 32);
}


VECTOR_HAMT_T::~VECTOR_HAMT_T(){
	QUARK_ASSERT(check_invariant());
}

bool VECTOR_HAMT_T::check_invariant() const {
	QUARK_ASSERT(this->alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_vector());
	QUARK_ASSERT(get_debug_info(alloc) == "vechamt");
	const auto& vec = get_vecref();
//	QUARK_ASSERT(vec.impl().shift == 5);
	QUARK_ASSERT(vec.impl().check_tree());
	return true;
}

runtime_value_t alloc_vector_hamt(heap_t& heap, uint64_t allocation_count, uint64_t element_count, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(value_type.is_vector());

	auto vec = reinterpret_cast<VECTOR_HAMT_T*>(alloc_64(heap, 0, value_type, "vechamt"));

	QUARK_ASSERT(sizeof(immer::vector<runtime_value_t>) <= heap_alloc_64_t::k_data_bytes);
    new (&vec->alloc.data[0]) immer::vector<runtime_value_t>(allocation_count, runtime_value_t{ .int_value = (int64_t)0xdeadbeef12345678 } );

	QUARK_ASSERT(vec->check_invariant());
	QUARK_ASSERT(heap.check_invariant());

	return { .vector_hamt_ptr = vec };
}

runtime_value_t alloc_vector_hamt(heap_t& heap, const runtime_value_t elements[], uint64_t element_count, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(element_count == 0 || elements != nullptr);
	QUARK_ASSERT(value_type.is_vector());

	heap_alloc_64_t* alloc = alloc_64(heap, 0, value_type, "vechamt");

	auto vec = reinterpret_cast<VECTOR_HAMT_T*>(alloc);
	auto buffer_ptr = reinterpret_cast<immer::vector<runtime_value_t>*>(&alloc->data[0]);

	QUARK_ASSERT(sizeof(immer::vector<runtime_value_t>) <= heap_alloc_64_t::k_data_bytes);
    auto vec2 = new (buffer_ptr) immer::vector<runtime_value_t>(&elements[0], &elements[element_count]);
	QUARK_ASSERT(vec2 == buffer_ptr);

	QUARK_ASSERT(vec->check_invariant());
	QUARK_ASSERT(heap.check_invariant());

	return { .vector_hamt_ptr = vec };
}

void dispose_vector_hamt(const runtime_value_t& vec){
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(sizeof(VECTOR_HAMT_T) == sizeof(heap_alloc_64_t));
	QUARK_ASSERT(vec.vector_hamt_ptr != nullptr);
	QUARK_ASSERT(vec.vector_hamt_ptr->check_invariant());

	auto& vec2 = vec.vector_hamt_ptr->get_vecref_mut();
	vec2.~vector<runtime_value_t>();

	auto heap = vec.vector_hamt_ptr->alloc.heap;
	dispose_alloc(vec.vector_hamt_ptr->alloc);
	QUARK_ASSERT(heap->check_invariant());
}

runtime_value_t store_immutable_hamt(const runtime_value_t& vec0, const uint64_t index, runtime_value_t value){
	QUARK_ASSERT(vec0.check_invariant());
	const auto& vec1 = *vec0.vector_hamt_ptr;
	QUARK_ASSERT(index < vec1.get_element_count());
	auto& heap = *vec1.alloc.heap;

	heap_alloc_64_t* alloc = alloc_64(heap, 0, vec0.vector_hamt_ptr->alloc.debug_value_type, "vechamt");

	auto vec = reinterpret_cast<VECTOR_HAMT_T*>(alloc);

	auto buffer_ptr = reinterpret_cast<immer::vector<runtime_value_t>*>(&alloc->data[0]);

	QUARK_ASSERT(sizeof(immer::vector<runtime_value_t>) <= heap_alloc_64_t::k_data_bytes);
    auto vec2 = new (buffer_ptr) immer::vector<runtime_value_t>();

	const auto& v2 = vec1.get_vecref().set(index, value);
	*vec2 = v2;

	QUARK_ASSERT(vec->check_invariant());
	QUARK_ASSERT(heap.check_invariant());

	return { .vector_hamt_ptr = vec };
}

runtime_value_t push_back_immutable_hamt(const runtime_value_t& vec0, runtime_value_t value){
	QUARK_ASSERT(vec0.check_invariant());
	const auto& vec1 = *vec0.vector_hamt_ptr;
	auto& heap = *vec1.alloc.heap;

	heap_alloc_64_t* alloc = alloc_64(heap, 0, vec0.vector_hamt_ptr->alloc.debug_value_type, "vechamt");
	auto vec = reinterpret_cast<VECTOR_HAMT_T*>(alloc);
	auto buffer_ptr = reinterpret_cast<immer::vector<runtime_value_t>*>(&alloc->data[0]);

	QUARK_ASSERT(sizeof(immer::vector<runtime_value_t>) <= heap_alloc_64_t::k_data_bytes);
    auto vec2 = new (buffer_ptr) immer::vector<runtime_value_t>();

	const auto& v2 = vec1.get_vecref().push_back(value);
	*vec2 = v2;

	QUARK_ASSERT(vec->check_invariant());
	QUARK_ASSERT(heap.check_invariant());

	return { .vector_hamt_ptr = vec };
}



QUARK_TEST("VECTOR_HAMT_T", "", "", ""){
	const auto vec_struct_size2 = sizeof(immer::vector<int>);
	QUARK_VERIFY(vec_struct_size2 == 32);
}

/*
QUARK_TEST("VECTOR_HAMT_T", "", "", ""){
	auto backend = make_test_value_backend();
	detect_leaks(backend.heap);

	const runtime_value_t a[] = { make_runtime_int(1000), make_runtime_int(2000), make_runtime_int(3000) };
	auto v = alloc_vector_hamt(backend.heap, a, 3, make_vector(backend.types, type_t::make_double()));
	QUARK_VERIFY(v.vector_hamt_ptr != nullptr);

	QUARK_VERIFY(v.vector_hamt_ptr->get_element_count() == 3);
	QUARK_VERIFY(v.vector_hamt_ptr->load_element(0).int_value == 1000);
	QUARK_VERIFY(v.vector_hamt_ptr->load_element(1).int_value == 2000);
	QUARK_VERIFY(v.vector_hamt_ptr->load_element(2).int_value == 3000);

	if(dec_rc(v.vector_hamt_ptr->alloc) == 0){
		dispose_vector_hamt(v);
	}

	QUARK_VERIFY(backend.check_invariant());
	detect_leaks(backend.heap);
}
*/





////////////////////////////////		DICT_CPPMAP_T



QUARK_TEST("", "", "", ""){
	const auto size = sizeof(CPPMAP);
	QUARK_ASSERT(size == 24);
}

bool DICT_CPPMAP_T::check_invariant() const{
	QUARK_ASSERT(alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_dict());
	QUARK_ASSERT(get_debug_info(alloc) == "cppdict");
	return true;
}

uint64_t DICT_CPPMAP_T::size() const {
	QUARK_ASSERT(check_invariant());

	const auto& d = get_map();
	return d.size();
}

runtime_value_t alloc_dict_cppmap(heap_t& heap, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(value_type.check_invariant());
	QUARK_ASSERT(value_type.is_dict());

	heap_alloc_64_t* alloc = alloc_64(heap, 0, value_type, "cppdict");
	auto dict = reinterpret_cast<DICT_CPPMAP_T*>(alloc);

	auto& m = dict->get_map_mut();

	QUARK_ASSERT(sizeof(CPPMAP) <= heap_alloc_64_t::k_data_bytes);
    new (&m) CPPMAP();

	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(dict->check_invariant());

	return runtime_value_t { .dict_cppmap_ptr = dict };
}

void dispose_dict_cppmap(runtime_value_t& d){
	QUARK_ASSERT(sizeof(DICT_CPPMAP_T) == sizeof(heap_alloc_64_t));
	QUARK_ASSERT(d.dict_cppmap_ptr != nullptr);
	auto& dict = *d.dict_cppmap_ptr;

	QUARK_ASSERT(dict.check_invariant());

	dict.get_map_mut().~CPPMAP();
	auto heap = dict.alloc.heap;
	dispose_alloc(dict.alloc);
	QUARK_ASSERT(heap->check_invariant());
}






////////////////////////////////		DICT_HAMT_T



QUARK_TEST("", "", "", ""){
	const auto size = sizeof(HAMT_MAP);
	QUARK_ASSERT(size == 16);
}

bool DICT_HAMT_T::check_invariant() const{
	QUARK_ASSERT(alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_dict());
	QUARK_ASSERT(get_debug_info(alloc) == "hamtdic");
	return true;
}

uint64_t DICT_HAMT_T::size() const {
	QUARK_ASSERT(check_invariant());

	const auto& d = get_map();
	return d.size();
}

runtime_value_t alloc_dict_hamt(heap_t& heap, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(value_type.is_dict());

	heap_alloc_64_t* alloc = alloc_64(heap, 0, value_type, "hamtdic");
	auto dict = reinterpret_cast<DICT_HAMT_T*>(alloc);

	auto& m = dict->get_map_mut();

	QUARK_ASSERT(sizeof(HAMT_MAP) <= heap_alloc_64_t::k_data_bytes);
    new (&m) HAMT_MAP();

	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(dict->check_invariant());

	return runtime_value_t { .dict_hamt_ptr = dict };
}

void dispose_dict_hamt(runtime_value_t& d){
	QUARK_ASSERT(sizeof(DICT_HAMT_T) == sizeof(heap_alloc_64_t));
	QUARK_ASSERT(d.dict_hamt_ptr != nullptr);
	auto& dict = *d.dict_hamt_ptr;

	QUARK_ASSERT(dict.check_invariant());

	dict.get_map_mut().~HAMT_MAP();
	auto heap = dict.alloc.heap;
	dispose_alloc(dict.alloc);
	QUARK_ASSERT(heap->check_invariant());
}










////////////////////////////////		JSON_T



bool JSON_T::check_invariant() const{
	QUARK_ASSERT(alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_json());
	QUARK_ASSERT(get_debug_info(alloc) == "JSON");
	QUARK_ASSERT(get_json().check_invariant());
	return true;
}

JSON_T* alloc_json(heap_t& heap, const json_t& init){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(init.check_invariant());

	heap_alloc_64_t* alloc = alloc_64(heap, 0, type_t::make_json(), "JSON");

	auto json = reinterpret_cast<JSON_T*>(alloc);
	auto copy = new json_t(init);
	json->alloc.data[0] = reinterpret_cast<uint64_t>(copy);

	QUARK_ASSERT(json->check_invariant());
	QUARK_ASSERT(heap.check_invariant());
	return json;
}

static void dispose_json(JSON_T& json){
	QUARK_ASSERT(sizeof(JSON_T) == sizeof(heap_alloc_64_t));
	QUARK_ASSERT(json.check_invariant());

	delete &json.get_json();
	json.alloc.data[0] = 0x00000000'00000000;

	auto heap = json.alloc.heap;
	dispose_alloc(json.alloc);
	QUARK_ASSERT(heap->check_invariant());
}





////////////////////////////////		STRUCT_T


//??? Store small structs inplace.


STRUCT_T::~STRUCT_T(){
	QUARK_ASSERT(check_invariant());
}

bool STRUCT_T::check_invariant() const {
	QUARK_ASSERT(this->alloc.check_invariant());
	QUARK_ASSERT(alloc.debug_value_type.is_struct());
	QUARK_ASSERT(get_debug_info(alloc) == "struct");
	return true;
}

STRUCT_T* alloc_struct(heap_t& heap, std::size_t size, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(value_type.check_invariant());
	QUARK_ASSERT(value_type.is_struct());

	const auto allocation_count = size_to_allocation_blocks(size);

	heap_alloc_64_t* alloc = alloc_64(heap, allocation_count, value_type, "struct");

	auto vec = reinterpret_cast<STRUCT_T*>(alloc);
	return vec;
}

STRUCT_T* alloc_struct_copy(heap_t& heap, const uint64_t data[], std::size_t size, type_t value_type){
	QUARK_ASSERT(heap.check_invariant());
	QUARK_ASSERT(data != nullptr);
	QUARK_ASSERT(value_type.check_invariant());

	const auto allocation_count = size_to_allocation_blocks(size);

	heap_alloc_64_t* alloc = alloc_64(heap, allocation_count, value_type, "struct");

	auto vec = reinterpret_cast<STRUCT_T*>(alloc);
	std::memcpy(vec->get_data_ptr(), data, size);
	return vec;
}

static void dispose_struct(STRUCT_T& s){
	QUARK_ASSERT(sizeof(STRUCT_T) == sizeof(heap_alloc_64_t));
	QUARK_ASSERT(s.check_invariant());

	auto heap = s.alloc.heap;
	dispose_alloc(s.alloc);
	QUARK_ASSERT(heap->check_invariant());
}




//??? If we stuff child[0].get_base_type() into the data-integer, we don't need types here.
bool is_rc_value(const types_t& types, const type_t& type){
	const auto physical_type = peek2(types, type);
	return
		physical_type.is_string()
		|| physical_type.is_vector()
		|| physical_type.is_dict()
		|| physical_type.is_struct()
		|| physical_type.is_json()
//		|| physical_type.is_function()
	;
}



// IMPORTANT: Different types will access different number of bytes, for example a BYTE. We cannot dereference pointer as a uint64*!!
runtime_value_t load_via_ptr2(const types_t& types, const void* value_ptr, const type_t& type){
	QUARK_ASSERT(value_ptr != nullptr);
	QUARK_ASSERT(type.check_invariant());

	struct visitor_t {
		const void* value_ptr;

		runtime_value_t operator()(const undefined_t& e) const{
			UNSUPPORTED();
		}
		runtime_value_t operator()(const any_t& e) const{
			UNSUPPORTED();
		}

		runtime_value_t operator()(const void_t& e) const{
			UNSUPPORTED();
		}
		runtime_value_t operator()(const bool_t& e) const{
			const auto temp = *static_cast<const uint8_t*>(value_ptr);
			return make_runtime_bool(temp == 0 ? false : true);
		}
		runtime_value_t operator()(const int_t& e) const{
			const auto temp = *static_cast<const uint64_t*>(value_ptr);
			return make_runtime_int(temp);
		}
		runtime_value_t operator()(const double_t& e) const{
			const auto temp = *static_cast<const double*>(value_ptr);
			return make_runtime_double(temp);
		}
		runtime_value_t operator()(const string_t& e) const{
			return *static_cast<const runtime_value_t*>(value_ptr);
		}

		runtime_value_t operator()(const json_type_t& e) const{
			return *static_cast<const runtime_value_t*>(value_ptr);
		}
		runtime_value_t operator()(const typeid_type_t& e) const{
			const auto value = *static_cast<const int32_t*>(value_ptr);
			return make_runtime_typeid(type_t(value));
		}

		runtime_value_t operator()(const struct_t& e) const{
			STRUCT_T* struct_ptr = *reinterpret_cast<STRUCT_T* const *>(value_ptr);
			return make_runtime_struct(struct_ptr);
		}
		runtime_value_t operator()(const vector_t& e) const{
			return *static_cast<const runtime_value_t*>(value_ptr);
		}
		runtime_value_t operator()(const dict_t& e) const{
			return *static_cast<const runtime_value_t*>(value_ptr);
		}
		runtime_value_t operator()(const function_t& e) const{
			return *static_cast<const runtime_value_t*>(value_ptr);
		}
		runtime_value_t operator()(const symbol_ref_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
		runtime_value_t operator()(const named_type_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
	};
	return std::visit(visitor_t{ value_ptr }, get_type_variant(types, type));
}

// IMPORTANT: Different types will access different number of bytes, for example a BYTE. We cannot dereference pointer as a uint64*!!
void store_via_ptr2(const types_t& types, void* value_ptr, const type_t& type, const runtime_value_t& value){
	struct visitor_t {
		void* value_ptr;
		const runtime_value_t& value;

		void operator()(const undefined_t& e) const{
			UNSUPPORTED();
		}
		void operator()(const any_t& e) const{
			UNSUPPORTED();
		}

		void operator()(const void_t& e) const{
			UNSUPPORTED();
		}
		void operator()(const bool_t& e) const{
			*static_cast<uint8_t*>(value_ptr) = value.bool_value;
		}
		void operator()(const int_t& e) const{
			*(int64_t*)value_ptr = value.int_value;
		}
		void operator()(const double_t& e) const{
			*static_cast<double*>(value_ptr) = value.double_value;
		}
		void operator()(const string_t& e) const{
			*static_cast<runtime_value_t*>(value_ptr) = value;
		}

		void operator()(const json_type_t& e) const{
			*static_cast<runtime_value_t*>(value_ptr) = value;
		}
		void operator()(const typeid_type_t& e) const{
			*static_cast<int32_t*>(value_ptr) = static_cast<int32_t>(value.typeid_itype);
		}

		void operator()(const struct_t& e) const{
			*reinterpret_cast<STRUCT_T**>(value_ptr) = value.struct_ptr;
		}
		void operator()(const vector_t& e) const{
			*static_cast<runtime_value_t*>(value_ptr) = value;
		}
		void operator()(const dict_t& e) const{
			*static_cast<runtime_value_t*>(value_ptr) = value;
		}
		void operator()(const function_t& e) const{
			*static_cast<runtime_value_t*>(value_ptr) = value;
		}
		void operator()(const symbol_ref_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
		void operator()(const named_type_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
	};
	std::visit(visitor_t{ value_ptr, value }, get_type_variant(types, type));
}


////////////////////////////////////////////			rt_value_t



rt_value_t::rt_value_t() :
	_backend(nullptr),
	_type(type_t::make_undefined()),
	_pod(make_blank_runtime_value())
{
	QUARK_ASSERT(check_invariant());
}

rt_value_t::~rt_value_t(){
	QUARK_ASSERT(check_invariant());

	if(_backend){
		release_value(*_backend, _pod, _type);
	}
	else{
//		QUARK_ASSERT(is_rc_value(_backend->types, _type) == false);
	}
}

rt_value_t::rt_value_t(const rt_value_t& other) :
	_backend(other._backend),
	_type(other._type),
	_pod(other._pod)
{
	QUARK_ASSERT(other.check_invariant());


	if(_backend){
		retain_value(*_backend, _pod, _type);
	}
	else{
//		QUARK_ASSERT(is_rc_value(_backend->types, _type) == false);
	}

	QUARK_ASSERT(check_invariant());
}

rt_value_t& rt_value_t::operator=(const rt_value_t& other){
	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());

	rt_value_t temp(other);
	temp.swap(*this);

	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());
	return *this;
}

void rt_value_t::swap(rt_value_t& other){
	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());

	std::swap(_backend, other._backend);
	std::swap(_type, other._type);
	std::swap(_pod, other._pod);

	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());
}

rt_value_t::rt_value_t(const bc_static_frame_t* frame_ptr) :
	_backend(nullptr),
	_type(type_t::make_void())
{
	_pod = runtime_value_t{ .frame_ptr = (void*)frame_ptr };
	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_undefined(){
	return rt_value_t();
}

rt_value_t rt_value_t::make_any(){
	return rt_value_t();
}

rt_value_t rt_value_t::make_void(){
	return rt_value_t();
}

rt_value_t rt_value_t::make_bool(bool v){
	return rt_value_t(v);
}
bool rt_value_t::get_bool_value() const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(_type.is_bool());

	return _pod.bool_value;
}
rt_value_t::rt_value_t(bool value) :
	_backend(nullptr),
	_type(type_t::make_bool())
{
	_pod = make_runtime_bool(value);
	QUARK_ASSERT(check_invariant());
}


rt_value_t rt_value_t::make_int(int64_t v){
	return rt_value_t{ v };
}
int64_t rt_value_t::get_int_value() const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(_type.is_int());

	return _pod.int_value;
}
rt_value_t::rt_value_t(int64_t value) :
	_backend(nullptr),
	_type(type_t::make_int())
{
	_pod = make_runtime_int(value);
	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_double(double v){
	return rt_value_t{ v };
}
double rt_value_t::get_double_value() const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(_type.is_double());

	return _pod.double_value;
}
rt_value_t::rt_value_t(double value) :
	_backend(nullptr),
	_type(type_t::make_double())
{
	_pod.double_value = value;
	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_string(value_backend_t& backend, const std::string& v){
	QUARK_ASSERT(backend.check_invariant());

	return rt_value_t{ backend, v };
}
std::string rt_value_t::get_string_value(const value_backend_t& backend) const{
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant());

	return from_runtime_string2(backend, _pod);
}
rt_value_t::rt_value_t(value_backend_t& backend, const std::string& value) :
	_backend(&backend),
	_type(type_t::make_string()),
	_pod(to_runtime_string2(backend, value))
{
	QUARK_ASSERT(backend.check_invariant());

	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_json(value_backend_t& backend, const json_t& v){
	return rt_value_t{ backend, std::make_shared<json_t>(v) };
}
json_t rt_value_t::get_json() const{
	QUARK_ASSERT(check_invariant());

	if(_pod.json_ptr == nullptr){
		return json_t();
	}
	else{
		return _pod.json_ptr->get_json();
	}
}
rt_value_t::rt_value_t(value_backend_t& backend, const std::shared_ptr<json_t>& value) :
	_backend(&backend),
	_type(type_t::make_json())
{
	QUARK_ASSERT(value);
	QUARK_ASSERT(value->check_invariant());

	_pod.json_ptr = alloc_json(backend.heap, *value);

	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_typeid_value(const type_t& type_id){
	return rt_value_t{ type_id };
}
type_t rt_value_t::get_typeid_value() const {
	QUARK_ASSERT(check_invariant());

	return type_t(_pod.typeid_itype);
}
rt_value_t::rt_value_t(const type_t& type_id) :
	_backend(nullptr),
	_type(type_desc_t::make_typeid())
{
	QUARK_ASSERT(type_id.check_invariant());

	_pod = make_runtime_typeid(type_id);

	QUARK_ASSERT(check_invariant());
}


std::vector<rt_value_t> from_runtime_struct(value_backend_t& backend, const runtime_value_t encoded_value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(encoded_value.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& type_peek = peek2(backend.types, type);

	const auto& struct_layout = find_struct_layout(backend, type_peek);
	const auto& struct_def = type_peek.get_struct(backend.types);
	const auto struct_base_ptr = encoded_value.struct_ptr->get_data_ptr();

	std::vector<rt_value_t> members;
	int member_index = 0;
	for(const auto& e: struct_def._members){
		const auto offset = struct_layout.second.members[member_index].offset;
		const auto member_ptr = reinterpret_cast<const runtime_value_t*>(struct_base_ptr + offset);
		const auto member_value = rt_value_t(backend, e._type, *member_ptr, rt_value_t::rc_mode::bump);
		members.push_back(member_value);
		member_index++;
	}
	return members;
}

runtime_value_t to_runtime_struct(value_backend_t& backend, const type_t& struct_type, const std::vector<rt_value_t>& values){
	QUARK_ASSERT(backend.check_invariant());

	const auto& struct_layout = find_struct_layout(backend, struct_type);

	auto s = alloc_struct(backend.heap, struct_layout.second.size, peek2(backend.types, struct_type));
	const auto struct_base_ptr = s->get_data_ptr();

	int member_index = 0;
	for(const auto& e: values){
		const auto offset = struct_layout.second.members[member_index].offset;
		const auto member_ptr = reinterpret_cast<void*>(struct_base_ptr + offset);
		const auto member_type = e._type;

		retain_value(backend, e._pod, e._type);
		store_via_ptr2(backend.types, member_ptr, dereference_type(backend.types, member_type), e._pod);
		member_index++;
	}
	return make_runtime_struct(s);
}



rt_value_t rt_value_t::make_struct_value(value_backend_t& backend, const type_t& struct_type, const std::vector<rt_value_t>& values){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(struct_type.check_invariant());

	return rt_value_t{ backend, struct_type, values, true };
}
const std::vector<rt_value_t> rt_value_t::get_struct_value(value_backend_t& backend) const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(backend.check_invariant());
//	QUARK_ASSERT(_type.is_struct());

	const auto member_values = from_runtime_struct(backend, _pod, _type);
	return member_values;
}
rt_value_t::rt_value_t(value_backend_t& backend, const type_t& struct_type, const std::vector<rt_value_t>& values, bool struct_tag) :
	_backend(&backend),
	_type(struct_type)
{
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(struct_type.check_invariant());
#if QUARK_ASSERT_ON
//??? check type of each member.
	for(const auto& e: values) {
		QUARK_ASSERT(e.check_invariant());
	}
#endif

	_pod = to_runtime_struct(backend, struct_type, values);
	QUARK_ASSERT(check_invariant());
}

rt_value_t rt_value_t::make_function_value(value_backend_t& backend, const type_t& function_type, const module_symbol_t& function_id){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(function_type.is_function());

	return rt_value_t{ backend, function_type, function_id };
}
int64_t rt_value_t::get_function_value() const{
	QUARK_ASSERT(check_invariant());

	return _pod.function_link_id;
}
rt_value_t::rt_value_t(value_backend_t& backend, const type_t& function_type, const module_symbol_t& function_id) :
	_backend(nullptr),
	_type(function_type)
{
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(function_type.is_function());

	const auto id = find_function_by_name0(backend, function_id);
	_pod = runtime_value_t { .function_link_id = id };

	QUARK_ASSERT(check_invariant());
}

rt_value_t::rt_value_t(value_backend_t& backend, const type_t& type, const runtime_value_t& internals, rc_mode mode) :
	_backend(&backend),
	_type(type),
	_pod(internals)
{
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	if(mode == rc_mode::adopt){
	}
	else if(mode == rc_mode::bump){
		retain_value(backend, internals, type);
	}
	else{
		QUARK_ASSERT(false);
	}

	QUARK_ASSERT(check_invariant());
}
rt_value_t::rt_value_t(const type_t& type, const runtime_value_t& internals) :
	_backend(nullptr),
	_type(type),
	_pod(internals)
{
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(check_invariant());
}


#if DEBUG
bool rt_value_t::check_invariant() const {
	QUARK_ASSERT(_type.check_invariant());

	if(_type.is_function()){
		QUARK_ASSERT(_pod.function_link_id == -1 || (_pod.function_link_id >= 0 && _pod.function_link_id < 10000));
	}

	if(_backend != nullptr){
		QUARK_ASSERT(floyd::check_invariant(*_backend, _pod, _type));
	}

	return true;
}
#endif

rt_value_t::rt_value_t(const type_t& type, mode mode) :
	_backend(nullptr),
	_type(type)
{
	QUARK_ASSERT(type.check_invariant());

	//	Allocate a dummy external value.
//	auto temp = new bc_external_value_t{"UNWRITTEN EXT VALUE"};
#if DEBUG
//	temp->_debug__is_unwritten_external_value = true;
#endif
	_pod = make_blank_runtime_value();

	QUARK_ASSERT(check_invariant());
}


QUARK_TEST("rt_value_t", "", "", ""){
	const auto a = rt_value_t();
}
QUARK_TEST("rt_value_t", "", "", ""){
	const auto a = rt_value_t::make_int(200);
	QUARK_VERIFY(a.get_int_value() == 200);
}




//??? No need to make a new immer::vector if values already sit inside on, in the VEC_T!
//??? No need to store type in each element...
const immer::vector<rt_value_t> get_vector_elements(value_backend_t& backend, const rt_value_t& value0){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value0.check_invariant());

	const auto type = value0._type;

	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_vector());

	if(is_vector_carray(backend.types, backend.config, type)){
		const auto element_type = type_peek.get_vector_element_type(backend.types);
		const auto vec = value0._pod.vector_carray_ptr;

		immer::vector<rt_value_t> elements;
		const auto count = vec->get_element_count();
		auto p = vec->get_element_ptr();
		for(int i = 0 ; i < count ; i++){
			const auto value_encoded = p[i];
			const auto value = rt_value_t(backend, element_type, value_encoded, rt_value_t::rc_mode::bump);
			elements = elements.push_back(value);
		}
		return elements;
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		const auto element_type = type_peek.get_vector_element_type(backend.types);
		const auto vec = value0._pod.vector_hamt_ptr;

		immer::vector<rt_value_t> elements;
		const auto count = vec->get_element_count();
		for(int i = 0 ; i < count ; i++){
			const auto& value_encoded = vec->load_element(i);
			const auto value = rt_value_t(backend, element_type, value_encoded, rt_value_t::rc_mode::bump);
			elements = elements.push_back(value);
		}
		return elements;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

rt_value_t make_vector_value(value_backend_t& backend, const type_t& element_type, const immer::vector<rt_value_t>& v0){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(element_type.check_invariant());
#if QUARK_ASSERT_ON
	for(const auto& e: v0) {
		QUARK_ASSERT(e.check_invariant());
	}
#endif

	const auto type = type_t::make_vector(backend.types, element_type);
	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_vector());

	const auto count = v0.size();

	if(is_vector_carray(backend.types, backend.config, type)){
		auto result = alloc_vector_carray(backend.heap, count, count, type);

		auto p = result.vector_carray_ptr->get_element_ptr();
		for(int i = 0 ; i < count ; i++){
			const auto& e = v0[i];
			QUARK_ASSERT(e.check_invariant());
			QUARK_ASSERT(e._type == element_type);

			p[i] = e._pod;
			retain_value(backend, e._pod, element_type);
		}
		const auto result2 = rt_value_t(backend, type, result, rt_value_t::rc_mode::adopt);
		QUARK_ASSERT(result2.check_invariant());
		return result2;
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		std::vector<runtime_value_t> temp;
		for(int i = 0 ; i < count ; i++){
			const auto& e = v0[i];
			QUARK_ASSERT(e.check_invariant());
//			QUARK_ASSERT(e._type == element_type);

			retain_value(backend, e._pod, element_type);
			temp.push_back(e._pod);
		}
		auto result = alloc_vector_hamt(backend.heap, &temp[0], temp.size(), type);
		const auto result2 = rt_value_t(backend, type, result, rt_value_t::rc_mode::adopt);
		QUARK_ASSERT(result2.check_invariant());
		return result2;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

const immer::map<std::string, rt_value_t> get_dict_values(value_backend_t& backend, const rt_value_t& dict0){
	const auto& type = dict0._type;
	const auto& type_peek = peek2(backend.types, type);

	const auto value_type = type_peek.get_dict_value_type(backend.types);
	if(is_dict_cppmap(backend.types, backend.config, type)){
		const auto dict = dict0._pod.dict_cppmap_ptr;
		immer::map<std::string, rt_value_t> values;
		const auto& map2 = dict->get_map();
		for(const auto& e: map2){
			QUARK_ASSERT(e.second.check_invariant());

			const auto value = rt_value_t(backend, value_type, e.second, rt_value_t::rc_mode::bump);
			values = values.insert({ e.first, value} );
		}
		return values;
	}
	else if(is_dict_hamt(backend.types, backend.config, type)){
		const auto dict = dict0._pod.dict_hamt_ptr;
		immer::map<std::string, rt_value_t> values;
		const auto& map2 = dict->get_map();
		for(const auto& e: map2){
			QUARK_ASSERT(e.second.check_invariant());

			const auto value = rt_value_t(backend, value_type, e.second, rt_value_t::rc_mode::bump);
			values = values.insert({ e.first, value} );
		}
		return values;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

rt_value_t make_dict_value(value_backend_t& backend, const type_t& value_type, const immer::map<std::string, rt_value_t>& entries){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value_type.check_invariant());

	const auto dict_type = type_t::make_dict(backend.types, value_type);
	const auto& type_peek = peek2(backend.types, dict_type);
	QUARK_ASSERT(type_peek.is_dict());

	if(is_dict_cppmap(backend.types, backend.config, dict_type)){
		auto result = alloc_dict_cppmap(backend.heap, type_peek);
		auto& m = result.dict_cppmap_ptr->get_map_mut();
		for(const auto& e: entries){
			QUARK_ASSERT(e.second.check_invariant());
			QUARK_ASSERT(e.second._type == value_type);

			retain_value(backend, e.second._pod, value_type);
			m.insert({ e.first, e.second._pod });
		}
		const auto result2 = rt_value_t(backend, dict_type, result, rt_value_t::rc_mode::adopt);
		QUARK_ASSERT(result2.check_invariant());
		return result2;
	}
	else if(is_dict_hamt(backend.types, backend.config, dict_type)){
		auto result = alloc_dict_hamt(backend.heap, type_peek);
		auto& m = result.dict_hamt_ptr->get_map_mut();
		for(const auto& e: entries){
			QUARK_ASSERT(e.second.check_invariant());
			QUARK_ASSERT(e.second._type == value_type);

			retain_value(backend, e.second._pod, value_type);
			m = m.set(e.first, e.second._pod);
		}
		const auto result2 = rt_value_t(backend, dict_type, result, rt_value_t::rc_mode::adopt);
		QUARK_ASSERT(result2.check_invariant());
		return result2;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

QUARK_TEST("", "", "", ""){
	const auto s = sizeof(rt_value_t);
	QUARK_VERIFY(s >= 8);
//	QUARK_VERIFY(s == 16);
}

int compare_value_true_deep(value_backend_t& backend, const rt_value_t& left, const rt_value_t& right, const type_t& type0){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(left._type == right._type);
	QUARK_ASSERT(left.check_invariant());
	QUARK_ASSERT(right.check_invariant());

//	trace_heap(backend.heap);
	const auto left_value = from_runtime_value2(backend, left._pod, type0);
	const auto right_value = from_runtime_value2(backend, right._pod, type0);
	const int result = value_t::compare_value_true_deep(backend.types, left_value, right_value);
	return result;
}





////////////////////////////////		rt_value_t JSON



json_t bcvalue_to_json(value_backend_t& backend, const rt_value_t& v){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(v.check_invariant());

	const auto& types = backend.types;

	const auto peek = peek2(backend.types, v._type);

	if(v._type.is_undefined()){
		return json_t();
	}
	else if(peek.is_any()){
		return json_t();
	}
	else if(peek.is_void()){
		return json_t();
	}
	else if(peek.is_bool()){
		return json_t(v.get_bool_value());
	}
	else if(peek.is_int()){
		return json_t(static_cast<double>(v.get_int_value()));
	}
	else if(peek.is_double()){
		return json_t(static_cast<double>(v.get_double_value()));
	}
	else if(peek.is_string()){
		if(v._pod.vector_carray_ptr == nullptr){
			return json_t("NULL PTR");
		}
		else{
			return json_t(v.get_string_value(backend));
		}
	}
	else if(peek.is_json()){
		return v.get_json();
	}
	else if(peek.is_typeid()){
		return type_to_json(types, v.get_typeid_value());
	}
	else if(peek.is_struct()){
		const auto& struct_value = v.get_struct_value(backend);
		std::map<std::string, json_t> obj2;
		const auto& struct_def = peek.get_struct(types);
		for(int i = 0 ; i < struct_def._members.size() ; i++){
			const auto& member = struct_def._members[i];
			const auto& key = member._name;
			const auto& value = struct_value[i];
			const auto& value2 = bcvalue_to_json(backend, value);
			obj2[key] = value2;
		}
		return json_t::make_object(obj2);
	}
	else if(peek.is_vector()){
		if(v._pod.vector_carray_ptr == nullptr){
			return json_t("NULL PTR");
		}
		else{
			const auto elements = get_vector_elements(backend, v);
			std::vector<json_t> result;
			for(const auto& e: elements){
				const auto j = bcvalue_to_json(backend, e);
				result.push_back(j);
			}
			return result;
		}
	}
	else if(peek.is_dict()){
		if(v._pod.vector_carray_ptr == nullptr){
			return json_t("NULL PTR");
		}
		else{
			const auto values = get_dict_values(backend, v);
			std::map<std::string, json_t> result;
			for(const auto& e: values){
				const auto j = bcvalue_to_json(backend, e.second);
				result.insert({e.first, j});
			}
			return result;
		}
	}
	else if(peek.is_function()){
		return json_t::make_object(
			{
				{ "funtyp", type_to_json(types, v._type) }
			}
		);
	}
	else{
		quark::throw_exception();
	}
}

json_t bcvalue_and_type_to_json(value_backend_t& backend, const rt_value_t& v){
	QUARK_ASSERT(backend.check_invariant());

	return json_t::make_array({
		type_to_json(backend.types, v._type),
		bcvalue_to_json(backend, v)
	});
}






bool is_struct_pod(const types_t& types, const struct_type_desc_t& struct_def){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(struct_def.check_invariant());

	for(const auto& e: struct_def._members){
		const auto& member_type = e._type;
		if(is_rc_value(types, member_type)){
			return false;
		}
	}
	return true;
}



int count_dyn_args(const types_t& types, const type_t& function_type){
	QUARK_ASSERT(function_type.is_function());
	const auto args2 = peek2(types, function_type).get_function_args(types);
	int dyn_arg_count = (int)std::count_if(args2.begin(), args2.end(), [&](const auto& e){ return peek2(types, e).is_any(); });
	return dyn_arg_count;
}

json_t func_link_to_json(const types_t& types, const func_link_t& def){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(def.check_invariant());

	return json_t::make_array({
		json_t(def.module_symbol.s),
		json_t(def.debug_type),
		json_t(type_to_compact_string(types, def.function_type)),
		json_t(def.dynamic_arg_count),
		json_t(def.is_bc_function),
		json_t(ptr_to_hexstring(def.f)),
	});
}

void trace_func_link(const types_t& types, const std::vector<func_link_t>& defs){
	QUARK_ASSERT(types.check_invariant());

	const auto vec = mapf<json_t>(defs, [&](const auto& e){ return func_link_to_json(types, e); });
	const auto vec2 = json_t::make_array(vec);
	QUARK_TRACE(json_to_pretty_string(vec2));
}


////////////////////////////////		value_backend_t


const func_link_t* find_function_by_name2(const value_backend_t& backend, const module_symbol_t& s){
	const auto& v = backend.func_link_lookup;
	const auto it = std::find_if(v.begin(), v.end(), [&] (auto& m) { return m.module_symbol == s; });
	if(it != v.end()){
		return &(*it);
	}
	else{
		return nullptr;
	}
}
int64_t find_function_by_name0(const value_backend_t& backend, const module_symbol_t& s){
	const auto& v = backend.func_link_lookup;
	const auto it = std::find_if(v.begin(), v.end(), [&] (auto& m) { return m.module_symbol == s; });
	if(it != v.end()){
		const auto result = (int64_t)(it - v.begin());
		return result;
	}
	else{
		return -1;
	}
}


const func_link_t& lookup_func_link_from_id(const value_backend_t& backend, runtime_value_t value){
	auto func_id = value.function_link_id;
	if(func_id == -1){
		throw std::runtime_error("");
	}
	QUARK_ASSERT(func_id >= 0 && func_id < backend.func_link_lookup.size());
	const auto& e = backend.func_link_lookup[func_id];
	return e;
}

const func_link_t& lookup_func_link_from_native(const value_backend_t& backend, runtime_value_t value){
	const void* f = (const void*)value.function_link_id;
	const auto& v = backend.func_link_lookup;
	const auto it = std::find_if(v.begin(), v.end(), [&] (auto& m) { return m.f == f; });
	if(it == v.end()){
		throw std::runtime_error("");
	}
	else{
		return *it;
	}
}

const func_link_t* lookup_func_link(const value_backend_t& backend, runtime_value_t value){
	auto func_id = value.function_link_id;
	if(func_id == -1){
		return nullptr;
	}
	else if(func_id >= 0 && func_id < backend.func_link_lookup.size()){
		const auto& e = backend.func_link_lookup[func_id];
		return &e;
	}

	const void* f = (const void*)value.function_link_id;
	const auto& v = backend.func_link_lookup;
	const auto it = std::find_if(v.begin(), v.end(), [&] (auto& m) { return m.f == f; });
	if(it != v.end()){
		return &(*it);
	}

	return nullptr;
}

const func_link_t& lookup_func_link_required(const value_backend_t& backend, runtime_value_t value){
	const auto result = lookup_func_link(backend, value);
	if(result == nullptr){
		throw std::runtime_error("");
	}
	return *result;
}



value_backend_t::value_backend_t(
	const std::vector<func_link_t>& func_link_lookup,
	const std::vector<std::pair<type_t, struct_layout_t>>& struct_layouts,
	const types_t& types,
	const config_t& config
) :
	heap(config.trace_allocs),
	types(types),
	func_link_lookup(func_link_lookup),
	struct_layouts(struct_layouts),
	config(config)
{
	QUARK_ASSERT(config.check_invariant());

	for(type_lookup_index_t i = 0 ; i < types.nodes.size() ; i++){
		const auto type0 = lookup_type_from_index(types, i);
		const auto peek = peek2(types, type0);
		if(peek.is_vector()){
			const auto& type = peek.get_vector_element_type(types);
			child_type.push_back(type);
		}
		else if(peek.is_dict()){
			const auto& type = peek.get_dict_value_type(types);
			child_type.push_back(type);
		}
		else{
			child_type.push_back(type_t::make_undefined());
		}
	}

	QUARK_ASSERT(check_invariant());
}

value_backend_t::~value_backend_t(){
	QUARK_ASSERT(check_invariant());
}


bool check_invariant(const value_backend_t& backend, runtime_value_t value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto type_peek = peek2(backend.types, type);

	//??? BOth the check for UNINITIALIZED_RUNTIME_VALUE and int_value == 0 are temporary kludges
	if(is_rc_value(backend.types, type_peek) && value.int_value != UNINITIALIZED_RUNTIME_VALUE && value.int_value != 0){
		if(type_peek.is_string()){
			QUARK_ASSERT(value.vector_carray_ptr != nullptr);
			QUARK_ASSERT(value.vector_carray_ptr->check_invariant());
		}
		else if(is_vector_carray(backend.types, backend.config, type)){
			QUARK_ASSERT(value.vector_carray_ptr != nullptr);
			QUARK_ASSERT(value.vector_carray_ptr->check_invariant());
		}
		else if(is_vector_hamt(backend.types, backend.config, type)){
			QUARK_ASSERT(value.vector_hamt_ptr != nullptr);
			QUARK_ASSERT(value.vector_hamt_ptr->check_invariant());
		}
		else if(is_dict_cppmap(backend.types, backend.config, type)){
			QUARK_ASSERT(value.dict_cppmap_ptr != nullptr);
			QUARK_ASSERT(value.dict_cppmap_ptr->check_invariant());
		}
		else if(is_dict_hamt(backend.types, backend.config, type)){
			QUARK_ASSERT(value.dict_hamt_ptr != nullptr);
			QUARK_ASSERT(value.dict_hamt_ptr->check_invariant());
		}
		else if(type_peek.is_json()){
			if(value.json_ptr != nullptr){
				QUARK_ASSERT(value.json_ptr->check_invariant());
			}
		}
		else if(type_peek.is_struct()){
			QUARK_ASSERT(value.struct_ptr != nullptr);
			QUARK_ASSERT(value.struct_ptr->check_invariant());
			const auto a = from_runtime_struct(backend, value, type);
			QUARK_ASSERT(a.check_invariant());
		}
		else{
			QUARK_ASSERT(false);
		}
	}
	else{
	}
	return true;
}



struct structure_t {
	base_type bt;
	int32_t rc;
	int64_t alloc_id;
	std::vector<structure_t> children;
};

static structure_t get_value_structure(const value_backend_t& backend, runtime_value_t value, const type_t& type0);

//??? make GP traversal function for values.
static std::vector<structure_t> get_value_structure_children(const value_backend_t& backend, runtime_value_t value, const type_t& type0){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, value, type0));
	QUARK_ASSERT(type0.check_invariant());

	const auto& type = peek2(backend.types, type0);

	if(type.is_struct()){
		std::vector<structure_t> children;
		const auto& struct_layout = find_struct_layout(backend, type);
		const auto& struct_def = type.get_struct(backend.types);
		const auto struct_base_ptr = value.struct_ptr->get_data_ptr();

		int member_index = 0;

		for(const auto& e: struct_def._members){
			const auto& member_peek = peek2(backend.types, e._type);
			if(is_rc_value(backend.types, member_peek)){
				const auto offset = struct_layout.second.members[member_index].offset;
				const auto member_ptr = reinterpret_cast<const runtime_value_t*>(struct_base_ptr + offset);
				const auto m = get_value_structure(backend, *member_ptr, member_peek);
				children.push_back(m);
			}
			member_index++;
		}
		return children;
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		std::vector<structure_t> children;
		const auto element_peek = type.get_vector_element_type(backend.types);
		if(is_rc_value(backend.types, element_peek)){
			const auto& obj = *value.vector_hamt_ptr;
			for(int i = 0 ; i < obj.get_element_count() ; i++){
				const auto& e = obj.load_element(i);
				const auto m = get_value_structure(backend, e, element_peek);
				children.push_back(m);
			}
		}
		return children;
	}
	else if(is_vector_carray(backend.types, backend.config, type)){
		std::vector<structure_t> children;
		const auto element_peek = type.get_vector_element_type(backend.types);
		if(is_rc_value(backend.types, element_peek)){
			const auto& obj = *value.vector_carray_ptr;
			for(int i = 0 ; i < obj.get_element_count() ; i++){
				const auto& e = obj.load_element(i);
				const auto m = get_value_structure(backend, e, element_peek);
				children.push_back(m);
			}
		}
		return children;
	}
	else if(is_dict_hamt(backend.types, backend.config, type)){
		std::vector<structure_t> children;
		const auto element_peek = type.get_dict_value_type(backend.types);
		if(is_rc_value(backend.types, element_peek)){
			const auto& obj = value.dict_hamt_ptr->get_map();
			for(const auto& kv: obj){
				const auto m = get_value_structure(backend, kv.second, element_peek);
				children.push_back(m);
			}
		}
		return children;
	}
	else if(is_dict_cppmap(backend.types, backend.config, type)){
		std::vector<structure_t> children;
		const auto element_peek = type.get_dict_value_type(backend.types);
		if(is_rc_value(backend.types, element_peek)){
			const auto& obj = value.dict_cppmap_ptr->get_map();
			for(const auto& kv: obj){
				const auto m = get_value_structure(backend, kv.second, element_peek);
				children.push_back(m);
			}
		}
		return children;
	}
	else if(type.is_json()){
		//	Notice: JSON stores its contents in the C++ world, not using value_backend_t.
		return {};
	}
	else{
		return {};
	}
}
static structure_t get_value_structure(const value_backend_t& backend, runtime_value_t value, const type_t& type0){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, value, type0));
	QUARK_ASSERT(type0.check_invariant());

	const auto& type = peek2(backend.types, type0);

	const auto is_rc = is_rc_value(backend.types, type);
	QUARK_ASSERT(is_rc);

	const auto alloc_id = value.gp_ptr->alloc_id;
	const int32_t rc = value.gp_ptr->rc;
	const auto children = get_value_structure_children(backend, value, type);
	return { type.get_base_type(), rc, alloc_id, children };
}

static std::string get_value_structure_str(const value_backend_t& backend, const structure_t& str){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(str.rc >= 0);
	QUARK_ASSERT(str.alloc_id >= k_alloc_start_id);
	std::stringstream s;

	const auto bt = base_type_to_opcode(str.bt);
	s << bt << " [" << str.alloc_id << " " << str.rc << "]" << "{";

	for(const auto& e: str.children){
		const auto a = get_value_structure_str(backend, e);
		s << a << " ";
	}
	s << "}";
	return s.str();
}



void trace_value_backend(const value_backend_t& backend){
}

static void trace_value_backend_dynamic__internal(const value_backend_t& backend){
	QUARK_ASSERT(backend.check_invariant());
	const auto& heap = backend.heap;

	std::vector<std::vector<std::string>> matrix;
	for(int i = 0 ; i < heap.alloc_records.size() ; i++){
		const auto& e = heap.alloc_records[i];
		QUARK_ASSERT(e.alloc_ptr->check_invariant());

		const auto alloc_id_str =std::to_string(e.alloc_ptr->alloc_id);
		const auto magic = value_to_hex_string(e.alloc_ptr->magic, 8);

		//		<< "used: " << e.in_use

		const auto rc_str = std::to_string(e.alloc_ptr->rc);

		const auto debug_info_str = get_debug_info(*e.alloc_ptr);

		const auto debug_value_type = e.alloc_ptr->debug_value_type;
		const auto debug_value_type_str = type_to_compact_string(backend.types, debug_value_type);

/*
		const uint64_t data0 = e.alloc_ptr->data[0];
		const uint64_t data1 = e.alloc_ptr->data[1];
		const uint64_t data2 = e.alloc_ptr->data[2];
		const uint64_t data3 = e.alloc_ptr->data[3];
*/

		const auto value = runtime_value_t{ .gp_ptr = e.alloc_ptr };
		std::string value_summary_str = get_value_structure_str(backend, get_value_structure(backend, value, debug_value_type));

		const auto is_rc = is_rc_value(backend.types, debug_value_type);
		QUARK_ASSERT(is_rc);

		const auto line = std::vector<std::string> {
			std::to_string(i),
			alloc_id_str,
			rc_str,
			magic,
			debug_value_type_str,
			value_summary_str
		};

		matrix.push_back(line);
	}

	const auto result = generate_table_type1({ "#", "alloc ID", "RC", "magic", "type", "value summary" }, matrix);
	QUARK_TRACE(result);
}


void trace_value_backend_dynamic(const value_backend_t& backend){
	QUARK_ASSERT(backend.check_invariant());

	QUARK_SCOPED_TRACE("BACKEND");

	if(backend.heap.record_allocs_flag && k_heap_mutex){
		std::lock_guard<std::recursive_mutex> guard(*backend.heap.alloc_records_mutex);
		trace_value_backend_dynamic__internal(backend);
	}
	else{
		trace_value_backend_dynamic__internal(backend);
	}
}




const std::pair<type_t, struct_layout_t>& find_struct_layout(const value_backend_t& backend, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& vec = backend.struct_layouts;
	const auto it = std::find_if(
		vec.begin(),
		vec.end(),
		[&] (auto& e) {
			return e.first == type;
		}
	);
	if(it != vec.end()){
		return *it;
	}
	else{
		throw std::exception();
	}
}

//??? keep hash from struct-type-ID -> layout
std::pair<runtime_value_t, type_t> load_struct_member(
	const value_backend_t& backend,
	uint8_t* data_ptr,
	const type_t& struct_type,
	int member_index
){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(struct_type.check_invariant());

	const auto peek = peek2(backend.types, struct_type);
	QUARK_ASSERT(peek.is_struct());
	const auto& struct_layout_info = find_struct_layout(backend, peek);

	QUARK_ASSERT(member_index >= 0 && member_index < struct_layout_info.second.members.size());
	const auto& member = struct_layout_info.second.members[member_index];
	void* member_ptr = data_ptr + member.offset;
	const auto value = *(runtime_value_t*)member_ptr;

	return { value, member.type };

}


////////////////////////////////		VALUES


value_backend_t make_test_value_backend(){
	return value_backend_t(
		{},
		{},
		types_t(),
		make_default_config()
	);
}



void retain_vector_carray(value_backend_t& backend, runtime_value_t vec, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(is_rc_value(backend.types, type));
	QUARK_ASSERT(is_vector_carray(backend.types, backend.config, type) || peek2(backend.types, type).is_string());

	inc_rc(vec.vector_carray_ptr->alloc);

	QUARK_ASSERT(backend.check_invariant());
}



void retain_dict_cppmap(value_backend_t& backend, runtime_value_t dict, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(is_rc_value(backend.types, type));
	QUARK_ASSERT(is_dict_cppmap(backend.types, backend.config, type));

	inc_rc(dict.dict_cppmap_ptr->alloc);

	QUARK_ASSERT(backend.check_invariant());
}
void retain_dict_hamt(value_backend_t& backend, runtime_value_t dict, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(is_rc_value(backend.types, type));
	QUARK_ASSERT(is_dict_hamt(backend.types, backend.config, type));

	inc_rc(dict.dict_hamt_ptr->alloc);

	QUARK_ASSERT(backend.check_invariant());
}

void retain_struct(value_backend_t& backend, runtime_value_t s, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, s, type));
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(is_rc_value(backend.types, type));
	QUARK_ASSERT(peek2(backend.types, type).is_struct());

	inc_rc(s.struct_ptr->alloc);

	QUARK_ASSERT(backend.check_invariant());
}

void retain_json(value_backend_t& backend, runtime_value_t s){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, s, type_t::make_json()));

	inc_rc(s.json_ptr->alloc);

	QUARK_ASSERT(backend.check_invariant());
}

void retain_value(value_backend_t& backend, runtime_value_t value, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto type_peek = peek2(backend.types, type);

	//??? BOth the check for UNINITIALIZED_RUNTIME_VALUE and int_value == 0 are temporary kludges
	if(is_rc_value(backend.types, type_peek) && value.int_value != UNINITIALIZED_RUNTIME_VALUE && value.int_value != 0){
		if(type_peek.is_string()){
			retain_vector_carray(backend, value, type);
		}
		else if(is_vector_carray(backend.types, backend.config, type)){
			retain_vector_carray(backend, value, type);
		}
		else if(is_vector_hamt(backend.types, backend.config, type)){
			retain_vector_hamt(backend, value, type);
		}
		else if(is_dict_cppmap(backend.types, backend.config, type)){
			retain_dict_cppmap(backend, value, type);
		}
		else if(is_dict_hamt(backend.types, backend.config, type)){
			retain_dict_hamt(backend, value, type);
		}
		else if(type_peek.is_json()){
			if(value.json_ptr != nullptr){
				inc_rc(value.json_ptr->alloc);
			}
		}
		else if(type_peek.is_struct()){
			retain_struct(backend, value, type);
		}
		else{
			QUARK_ASSERT(false);
		}
	}

	QUARK_ASSERT(backend.check_invariant());
}




void release_dict_cppmap(value_backend_t& backend, runtime_value_t dict0, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict0.check_invariant());
	QUARK_ASSERT(peek2(backend.types, type).is_dict());
	QUARK_ASSERT(is_dict_cppmap(backend.types, backend.config, type));

	auto& dict = *dict0.dict_cppmap_ptr;
	if(dec_rc(dict.alloc) == 0){

		//	Release all elements.
		const auto element_type2 = lookup_dict_value_type(backend, type);
		if(is_rc_value(backend.types, element_type2)){
			auto m = dict.get_map();
			for(const auto& e: m){
				release_value(backend, e.second, element_type2);
			}
		}
		dispose_dict_cppmap(dict0);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_dict_hamt(value_backend_t& backend, runtime_value_t dict0, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict0.check_invariant());
	QUARK_ASSERT(peek2(backend.types, type).is_dict());
	QUARK_ASSERT(is_dict_hamt(backend.types, backend.config, type));

	auto& dict = *dict0.dict_hamt_ptr;
	if(dec_rc(dict.alloc) == 0){

		//	Release all elements.
		const auto element_type2 = lookup_dict_value_type(backend, type);
		if(is_rc_value(backend.types, element_type2)){
			auto m = dict.get_map();
			for(const auto& e: m){
				release_value(backend, e.second, element_type2);
			}
		}
		dispose_dict_hamt(dict0);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_dict(value_backend_t& backend, runtime_value_t dict, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict.check_invariant());
	QUARK_ASSERT(peek2(backend.types, type).is_dict());

	if(is_dict_cppmap(backend.types, backend.config, type)){
		release_dict_cppmap(backend, dict, type);
	}
	else if(is_dict_hamt(backend.types, backend.config, type)){
		release_dict_hamt(backend, dict, type);
	}
	else{
		QUARK_ASSERT(false);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_vector_carray_pod(value_backend_t& backend, runtime_value_t vec, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	//??? Instead of peek2(), use cast_to_desc() that asserts type is a non name.
	const auto& peek = peek2(backend.types, type);
	QUARK_ASSERT(peek.is_string() || is_vector_carray(backend.types, backend.config, type));

	if(peek.is_vector()){
		QUARK_ASSERT(is_rc_value(backend.types, lookup_vector_element_type(backend, type)) == false);
	}

	if(dec_rc(vec.vector_carray_ptr->alloc) == 0){
		dispose_vector_carray(vec);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_vector_carray_nonpod(value_backend_t& backend, runtime_value_t vec, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(peek2(backend.types, type).is_string() || is_vector_carray(backend.types, backend.config, type));
	QUARK_ASSERT(is_rc_value(backend.types, lookup_vector_element_type(backend, type)) == true);

	if(dec_rc(vec.vector_carray_ptr->alloc) == 0){
		//	Release all elements.
		{
			const auto element_type = lookup_vector_element_type(backend, type);

			auto element_ptr = vec.vector_carray_ptr->get_element_ptr();
			for(int i = 0 ; i < vec.vector_carray_ptr->get_element_count() ; i++){
				const auto& element = element_ptr[i];
				release_value(backend, element, element_type);
			}
		}
		dispose_vector_carray(vec);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_vector_hamt_elements_internal(value_backend_t& backend, runtime_value_t vec, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(is_vector_hamt(backend.types, backend.config, type));

	const auto element_type = lookup_vector_element_type(backend, type);
	for(int i = 0 ; i < vec.vector_hamt_ptr->get_element_count() ; i++){
		const auto& element = vec.vector_hamt_ptr->load_element(i);
		release_value(backend, element, element_type);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_vec(value_backend_t& backend, runtime_value_t vec, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& peek = peek2(backend.types, type);
	QUARK_ASSERT(peek.is_string() || peek.is_vector());

	if(peek.is_string()){
		if(dec_rc(vec.vector_carray_ptr->alloc) == 0){
			//	String has no elements to release.

			dispose_vector_carray(vec);
		}
	}
	else if(is_vector_carray(backend.types, backend.config, type)){
		const auto element_type = lookup_vector_element_type(backend, type);
		if(is_rc_value(backend.types, element_type)){
			release_vector_carray_nonpod(backend, vec, type);
		}
		else{
			release_vector_carray_pod(backend, vec, type);
		}
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		const auto element_type = lookup_vector_element_type(backend, type);
		if(is_rc_value(backend.types, element_type)){
			release_vector_hamt_nonpod(backend, vec, type);
		}
		else{
			release_vector_hamt_pod(backend, vec, type);
		}
	}
	else{
		QUARK_ASSERT(false);
	}

	QUARK_ASSERT(backend.check_invariant());
}

//??? prep data, inluding member types -- so we don't need to acces struct_def and its type_t:s.
void release_struct(value_backend_t& backend, runtime_value_t str, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(str.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(peek2(backend.types, type).is_struct());

	auto s = str.struct_ptr;

	if(dec_rc(s->alloc) == 0){
		const auto& struct_def = peek2(backend.types, type).get_struct(backend.types);
		const auto struct_base_ptr = s->get_data_ptr();

		const auto& struct_layout = find_struct_layout(backend, type);

		int member_index = 0;
		for(const auto& e: struct_def._members){
			const auto member_type = e._type;
			if(is_rc_value(backend.types, member_type)){
				const auto offset = struct_layout.second.members[member_index].offset;
				const auto member_ptr = reinterpret_cast<const runtime_value_t*>(struct_base_ptr + offset);
				release_value(backend, *member_ptr, member_type);
			}
			member_index++;
		}
		dispose_struct(*s);
	}

	QUARK_ASSERT(backend.check_invariant());
}

void release_json(value_backend_t& backend, runtime_value_t s){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(s.check_invariant());
	QUARK_ASSERT(check_invariant(backend, s, type_t::make_json()));

	auto json = s.json_ptr;
	//	NOTICE: Floyd runtime() init will destruct globals, including json::null.
	if(json == nullptr){
	}
	else{
		QUARK_ASSERT(json != nullptr);
		if(dec_rc(json->alloc) == 0){
			dispose_json(*json);
		}
	}
}


void release_value(value_backend_t& backend, runtime_value_t value, type_t type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& peek = peek2(backend.types, type);

	//??? BOth the check for UNINITIALIZED_RUNTIME_VALUE and int_value == 0 are temporary kludges
	if(is_rc_value(backend.types, peek) && value.int_value != UNINITIALIZED_RUNTIME_VALUE && value.int_value != 0){
		if(peek.is_string()){
			release_vec(backend, value, type);
		}
		else if(peek.is_vector()){
			release_vec(backend, value, type);
		}
		else if(peek.is_dict()){
			release_dict(backend, value, type);
		}
		else if(peek.is_json()){
			release_json(backend, value);
		}
		else if(peek2(backend.types, type).is_struct()){
			release_struct(backend, value, type);
		}
		else{
		}
	}

	QUARK_ASSERT(backend.check_invariant());
}













//////////////////////////////////////////		runtime_value_t


runtime_value_t alloc_carray_8bit(value_backend_t& backend, const uint8_t data[], std::size_t count){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(data != nullptr || count == 0);

	const auto allocation_count = size_to_allocation_blocks(count);
	auto result = alloc_vector_carray(backend.heap, allocation_count, count, type_t::make_string());

	size_t char_pos = 0;
	int element_index = 0;
	uint64_t acc = 0;
	while(char_pos < count){
		const uint64_t ch = data[char_pos];
		const auto x = (char_pos & 7) * 8;
		acc = acc | (ch << x);
		char_pos++;

		if(((char_pos & 7) == 0) || (char_pos == count)){
			result.vector_carray_ptr->store(element_index, make_runtime_int(static_cast<int64_t>(acc)));
			element_index = element_index + 1;
			acc = 0;
		}
	}

	return result;
}

runtime_value_t to_runtime_string2(value_backend_t& backend, const std::string& s){
	QUARK_ASSERT(backend.check_invariant());

	const uint8_t* p = reinterpret_cast<const uint8_t*>(s.c_str());
	const auto result = alloc_carray_8bit(backend, p, s.size());
	QUARK_ASSERT(check_invariant(backend, result, type_t::make_string()));
	return result;
}


QUARK_TEST("VECTOR_CARRAY_T", "", "", ""){
	auto backend = make_test_value_backend();
	const auto a = to_runtime_string2(backend, "hello, world!");

	QUARK_VERIFY(a.vector_carray_ptr->get_element_count() == 13);

	//	int64_t	'hello, w'
//	QUARK_VERIFY(a.vector_carray_ptr->load_element(0).int_value == 0x68656c6c6f2c2077);
	QUARK_VERIFY(a.vector_carray_ptr->load_element(0).int_value == 0x77202c6f6c6c6568);

	//int_value	int64_t	'orld!\0\0\0'
//	QUARK_VERIFY(a.vector_carray_ptr->load_element(1).int_value == 0x6f726c6421000000);
	QUARK_VERIFY(a.vector_carray_ptr->load_element(1).int_value == 0x00000021646c726f);
}


// backend not used???
std::string from_runtime_string2(const value_backend_t& backend, runtime_value_t encoded_value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, encoded_value, type_t::make_string()));

	const size_t size = get_vec_string_size(encoded_value);

	std::string result;

	//	Read 8 characters at a time.
	size_t char_pos = 0;
	const auto begin0 = encoded_value.vector_carray_ptr->begin();
	const auto end0 = encoded_value.vector_carray_ptr->end();
	for(auto it = begin0 ; it != end0 ; it++){
		const size_t copy_chars = std::min(size - char_pos, (size_t)8);
		const uint64_t element = it->int_value;
		for(int i = 0 ; i < copy_chars ; i++){
			const uint64_t x = i * 8;
			const uint64_t ch = (element >> x) & 0xff;
			result.push_back(static_cast<char>(ch));
		}
		char_pos += copy_chars;
	}

	QUARK_ASSERT(result.size() == size);
	return result;
}

QUARK_TEST("VECTOR_CARRAY_T", "", "", ""){
	auto backend = make_test_value_backend();
	const auto a = to_runtime_string2(backend, "hello, world!");

	const auto r = from_runtime_string2(backend, a);
	QUARK_VERIFY(r == "hello, world!");
}

runtime_value_t to_runtime_struct(value_backend_t& backend, const struct_t& exact_type, const value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(value.get_type().is_struct());

	const auto& struct_layout = find_struct_layout(backend, value.get_type());

	auto s = alloc_struct(backend.heap, struct_layout.second.size, value.get_type());
	const auto struct_base_ptr = s->get_data_ptr();

	int member_index = 0;
	const auto& struct_data = value.get_struct_value();

	for(const auto& e: struct_data->_member_values){
		const auto offset = struct_layout.second.members[member_index].offset;
		const auto member_ptr = reinterpret_cast<void*>(struct_base_ptr + offset);
		const auto member_type = e.get_type();
		store_via_ptr2(backend.types, member_ptr, member_type, to_runtime_value2(backend, e));
		member_index++;
	}
	return make_runtime_struct(s);
}

value_t from_runtime_struct(const value_backend_t& backend, const runtime_value_t encoded_value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
//	QUARK_ASSERT(check_invariant(backend, encoded_value, type));
	QUARK_ASSERT(type.check_invariant());

	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_struct());

	const auto& struct_layout = find_struct_layout(backend, type_peek);
	const auto& struct_def = type_peek.get_struct(backend.types);
	const auto struct_base_ptr = encoded_value.struct_ptr->get_data_ptr();

	std::vector<value_t> members;
	int member_index = 0;
	for(const auto& e: struct_def._members){
		const auto offset = struct_layout.second.members[member_index].offset;
		const auto member_ptr = reinterpret_cast<const runtime_value_t*>(struct_base_ptr + offset);
		const auto member_value = from_runtime_value2(backend, *member_ptr, e._type);
		members.push_back(member_value);
		member_index++;
	}
	return value_t::make_struct_value(backend.types, type_peek, members);
}


runtime_value_t to_runtime_vector(value_backend_t& backend, const value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(value.get_type().is_vector());

	const auto& type = value.get_type();
	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_vector());

	const auto& v0 = value.get_vector_value();
	const auto count = v0.size();

	if(is_vector_carray(backend.types, backend.config, type)){
		auto result = alloc_vector_carray(backend.heap, count, count, type);

//		const auto element_type = type_peek.get_vector_element_type(backend.types);
		auto p = result.vector_carray_ptr->get_element_ptr();
		for(int i = 0 ; i < count ; i++){
			const auto& e = v0[i];
			const auto a = to_runtime_value2(backend, e);
	//		retain_value(r, a, element_type);
			p[i] = a;
		}

		QUARK_ASSERT(check_invariant(backend, result, type));
		return result;
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		std::vector<runtime_value_t> temp;
		for(int i = 0 ; i < count ; i++){
			const auto& e = v0[i];
			const auto a = to_runtime_value2(backend, e);
			temp.push_back(a);
		}
		auto result = alloc_vector_hamt(backend.heap, &temp[0], temp.size(), type);
		QUARK_ASSERT(check_invariant(backend, result, type));
		return result;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

value_t from_runtime_vector(const value_backend_t& backend, const runtime_value_t encoded_value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, encoded_value, type));
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(type.is_vector());

	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_vector());

	if(is_vector_carray(backend.types, backend.config, type)){
		const auto element_type = type_peek.get_vector_element_type(backend.types);
		const auto vec = encoded_value.vector_carray_ptr;

		std::vector<value_t> elements;
		const auto count = vec->get_element_count();
		auto p = vec->get_element_ptr();
		for(int i = 0 ; i < count ; i++){
			const auto value_encoded = p[i];
			const auto value = from_runtime_value2(backend, value_encoded, element_type);
			elements.push_back(value);
		}
		const auto val = value_t::make_vector_value(backend.types, element_type, elements);
		return val;
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		const auto element_type = type_peek.get_vector_element_type(backend.types);
		const auto vec = encoded_value.vector_hamt_ptr;

		std::vector<value_t> elements;
		const auto count = vec->get_element_count();
		for(int i = 0 ; i < count ; i++){
			const auto& value_encoded = vec->load_element(i);
			const auto value = from_runtime_value2(backend, value_encoded, element_type);
			elements.push_back(value);
		}
		const auto val = value_t::make_vector_value(backend.types, element_type, elements);
		return val;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

runtime_value_t to_runtime_dict(value_backend_t& backend, const dict_t& exact_type, const value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(value.get_type().is_dict());

	const auto type = value.get_type();
	const auto& type_peek = peek2(backend.types, type);
	QUARK_ASSERT(type_peek.is_dict());

	if(is_dict_cppmap(backend.types, backend.config, type)){
		const auto& v0 = value.get_dict_value();

		auto result = alloc_dict_cppmap(backend.heap, type);

//		const auto element_type = type_peek.get_dict_value_type(backend.types);
		auto& m = result.dict_cppmap_ptr->get_map_mut();
		for(const auto& e: v0){
			const auto a = to_runtime_value2(backend, e.second);
			m.insert({ e.first, a });
		}
		QUARK_ASSERT(check_invariant(backend, result, type));
		return result;
	}
	else if(is_dict_hamt(backend.types, backend.config, type)){
		const auto& v0 = value.get_dict_value();

		auto result = alloc_dict_hamt(backend.heap, type);

//		const auto element_type = type_peek.get_dict_value_type(backend.types);
		auto& m = result.dict_hamt_ptr->get_map_mut();
		for(const auto& e: v0){
			const auto a = to_runtime_value2(backend, e.second);
			m = m.set(e.first, a);
		}
		QUARK_ASSERT(check_invariant(backend, result, type));
		return result;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

value_t from_runtime_dict(const value_backend_t& backend, const runtime_value_t encoded_value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, encoded_value, type));
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(type.is_dict());

	const auto& type_peek = peek2(backend.types, type);

	if(is_dict_cppmap(backend.types, backend.config, type)){
		const auto value_type = type_peek.get_dict_value_type(backend.types);
		const auto dict = encoded_value.dict_cppmap_ptr;

		std::map<std::string, value_t> values;
		const auto& map2 = dict->get_map();
		for(const auto& e: map2){
			const auto value = from_runtime_value2(backend, e.second, value_type);
			values.insert({ e.first, value} );
		}
		const auto val = value_t::make_dict_value(backend.types, value_type, values);
		return val;
	}
	else if(is_dict_hamt(backend.types, backend.config, type)){
		const auto value_type = type_peek.get_dict_value_type(backend.types);
		const auto dict = encoded_value.dict_hamt_ptr;

		std::map<std::string, value_t> values;
		const auto& map2 = dict->get_map();
		for(const auto& e: map2){
			const auto value = from_runtime_value2(backend, e.second, value_type);
			values.insert({ e.first, value} );
		}
		const auto val = value_t::make_dict_value(backend.types, value_type, values);
		return val;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

runtime_value_t to_runtime_value2(value_backend_t& backend, const value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());

	const auto type = value.get_type();

	struct visitor_t {
		value_backend_t& backend;
		const value_t& value;

		runtime_value_t operator()(const undefined_t& e) const{
			return make_blank_runtime_value();
		}
		runtime_value_t operator()(const any_t& e) const{
			return make_blank_runtime_value();
		}

		runtime_value_t operator()(const void_t& e) const{
			return make_blank_runtime_value();
		}
		runtime_value_t operator()(const bool_t& e) const{
			return make_runtime_bool(value.get_bool_value());
		}
		runtime_value_t operator()(const int_t& e) const{
			return make_runtime_int(value.get_int_value());
		}
		runtime_value_t operator()(const double_t& e) const{
			return make_runtime_double(value.get_double_value());
		}
		runtime_value_t operator()(const string_t& e) const{
			return to_runtime_string2(backend, value.get_string_value());
		}

		runtime_value_t operator()(const json_type_t& e) const{
//			auto result = new json_t(value.get_json());
//			return runtime_value_t { .json_ptr = result };
			auto result = alloc_json(backend.heap, value.get_json());
			return runtime_value_t { .json_ptr = result };
		}
		runtime_value_t operator()(const typeid_type_t& e) const{
			const auto t0 = value.get_typeid_value();
			return make_runtime_typeid(t0);
		}

		runtime_value_t operator()(const struct_t& e) const{
			return to_runtime_struct(backend, e, value);
		}
		runtime_value_t operator()(const vector_t& e) const{
			return to_runtime_vector(backend, value);
		}
		runtime_value_t operator()(const dict_t& e) const{
			return to_runtime_dict(backend, e, value);
		}
		runtime_value_t operator()(const function_t& e) const{
			const auto id = find_function_by_name0(backend, value.get_function_value());
			return runtime_value_t { .function_link_id = id };
		}
		runtime_value_t operator()(const symbol_ref_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
		runtime_value_t operator()(const named_type_t& e) const {
			const auto temp = value_t::replace_logical_type(value, peek2(backend.types, value.get_type()));
			const auto temp2 = to_runtime_value2(backend, temp);
			return temp2;
		}
	};
	const auto result = std::visit(visitor_t{ backend, value }, get_type_variant(backend.types, type));
	QUARK_ASSERT(check_invariant(backend, result, type));
	return result;
}




value_t from_runtime_value2(const value_backend_t& backend, const runtime_value_t encoded_value, const type_t& type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(check_invariant(backend, encoded_value, type));
	QUARK_ASSERT(type.check_invariant());

	struct visitor_t {
		const value_backend_t& backend;
		const runtime_value_t& encoded_value;
		const type_t& type;

		value_t operator()(const undefined_t& e) const{
			UNSUPPORTED();
		}
		value_t operator()(const any_t& e) const{
			UNSUPPORTED();
		}

		value_t operator()(const void_t& e) const{
			UNSUPPORTED();
		}
		value_t operator()(const bool_t& e) const{
			return value_t::make_bool(encoded_value.bool_value == 0 ? false : true);
		}
		value_t operator()(const int_t& e) const{
			return value_t::make_int(encoded_value.int_value);
		}
		value_t operator()(const double_t& e) const{
			return value_t::make_double(encoded_value.double_value);
		}
		value_t operator()(const string_t& e) const{
			return value_t::make_string(from_runtime_string2(backend, encoded_value));
		}

		value_t operator()(const json_type_t& e) const{
			if(encoded_value.json_ptr == nullptr){
				return value_t::make_json(json_t());
			}
			else{
				const auto& j = encoded_value.json_ptr->get_json();
				return value_t::make_json(j);
			}
		}
		value_t operator()(const typeid_type_t& e) const{
			const auto& type1 = lookup_type_ref(backend, encoded_value.typeid_itype);
			const auto& type2 = value_t::make_typeid_value(type1);
			return type2;
		}

		value_t operator()(const struct_t& e) const{
			return from_runtime_struct(backend, encoded_value, type);
		}
		value_t operator()(const vector_t& e) const{
			return from_runtime_vector(backend, encoded_value, type);
		}
		value_t operator()(const dict_t& e) const{
			return from_runtime_dict(backend, encoded_value, type);
		}
		value_t operator()(const function_t& e) const{
			const auto func_link_ptr = lookup_func_link(backend, encoded_value);
			return value_t::make_function_value(backend.types, type, func_link_ptr ? func_link_ptr->module_symbol : k_no_module_symbol);
		}
		value_t operator()(const symbol_ref_t& e) const {
			QUARK_ASSERT(false); throw std::exception();
		}
		value_t operator()(const named_type_t& e) const {
			const auto result = from_runtime_value2(backend, encoded_value, e.destination_type);
			return value_t::replace_logical_type(result, type);
		}
	};
	const auto result = std::visit(visitor_t{ backend, encoded_value, type }, get_type_variant(backend.types, type));
	QUARK_ASSERT(result.get_type() == type);
	return result;
}


//////////////////////////////////////////		value_t vs rt_value_t


static runtime_value_t make_runtime_non_rc(const value_t& value){
	QUARK_ASSERT(value.check_invariant());

	const auto& type = value.get_type();
	QUARK_ASSERT(type.check_invariant());

/*
	if(bt == base_type::k_undefined){
		return make_blank_runtime_value();
	}
	else if(bt == base_type::k_any){
		return make_blank_runtime_value();
	}
	else if(bt == base_type::k_void){
		return make_blank_runtime_value();
	}
	else if(bt == base_type::k_bool){
		return rt_value_t::make_bool(value.get_bool_value());
	}
	else if(bt == base_type::k_bool){
		return rt_value_t::make_bool(value.get_bool_value());
	}
	else if(bt == base_type::k_int){
		return rt_value_t::make_int(value.get_int_value());
	}
	else if(bt == base_type::k_double){
		return rt_value_t::make_double(value.get_double_value());
	}

	else if(bt == base_type::k_string){
		return rt_value_t::make_string(value.get_string_value());
	}
	else if(bt == base_type::k_json){
		return rt_value_t::make_json(value.get_json());
	}
	else if(bt == base_type::k_typeid){
		return rt_value_t::make_typeid_value(value.get_typeid_value());
	}
	else if(bt == base_type::k_struct){
		return rt_value_t::make_struct_value(logical_type, values_to_bcs(types, value.get_struct_value()->_member_values));
	}
*/

	if(type.is_undefined()){
		return make_blank_runtime_value();
	}
	else if(type.is_any()){
		return make_blank_runtime_value();
	}
	else if(type.is_void()){
		return make_blank_runtime_value();
	}
	else if(type.is_bool()){
		return make_runtime_bool(value.get_bool_value());
	}
	else if(type.is_int()){
		return make_runtime_int(value.get_int_value());
	}
	else if(type.is_double()){
		return make_runtime_double(value.get_double_value());
	}
	else if(type.is_typeid()){
		const auto t0 = value.get_typeid_value();
		return make_runtime_typeid(t0);
	}
	else if(type.is_json() && value.get_json().is_null()){
		return runtime_value_t { .json_ptr = nullptr };
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}
rt_value_t make_non_rc(const value_t& value){
	QUARK_ASSERT(value.check_invariant());

	const auto r = make_runtime_non_rc(value);
	return rt_value_t(value.get_type(), r);
}


value_t rt_to_value(const value_backend_t& backend, const rt_value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());

	return from_runtime_value2(backend, value._pod, value._type);
}

rt_value_t value_to_rt(value_backend_t& backend, const value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());

	const auto a = to_runtime_value2(backend, value);
	return rt_value_t(backend, value.get_type(), a, rt_value_t::rc_mode::adopt);
}

rt_value_t make_rt_value(value_backend_t& backend, runtime_value_t value, const type_t& type, rt_value_t::rc_mode mode){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	return rt_value_t(backend, type, value, mode);
}

runtime_value_t get_rt_value(value_backend_t& backend, const rt_value_t& value){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(value.check_invariant());

	return value._pod;
}












static VECTOR_CARRAY_T* unpack_vector_carray_arg(const value_backend_t& backend, runtime_value_t arg, runtime_type_t arg_type){
	QUARK_ASSERT(backend.check_invariant());
#if DEBUG
	const auto& type = lookup_type_ref(backend, arg_type);
#endif
	QUARK_ASSERT(peek2(backend.types, type).is_vector());
	QUARK_ASSERT(arg.vector_carray_ptr != nullptr);
	QUARK_ASSERT(arg.vector_carray_ptr->check_invariant());

	return arg.vector_carray_ptr;
}

static DICT_CPPMAP_T* unpack_dict_cppmap_arg(const value_backend_t& backend, runtime_value_t arg, runtime_type_t arg_type){
	QUARK_ASSERT(backend.check_invariant());
#if DEBUG
	const auto& type = lookup_type_ref(backend, arg_type);
#endif
	QUARK_ASSERT(peek2(backend.types, type).is_dict());
	QUARK_ASSERT(arg.dict_cppmap_ptr != nullptr);

	QUARK_ASSERT(arg.dict_cppmap_ptr->check_invariant());

	return arg.dict_cppmap_ptr;
}








#if 0

//////////////////////////////////////////		COMPARE



static int compare(int64_t value){
	if(value < 0){
		return -1;
	}
	else if(value > 0){
		return 1;
	}
	else{
		return 0;
	}
}

static int bc_compare_string(const std::string& left, const std::string& right){
	// ??? Better if it doesn't use c_ptr since that is non-pure string handling.
	return compare(std::strcmp(left.c_str(), right.c_str()));
}

QUARK_TEST("bc_compare_string()", "", "", ""){
	ut_verify_auto(QUARK_POS, bc_compare_string("", ""), 0);
}
QUARK_TEST("bc_compare_string()", "", "", ""){
	ut_verify_auto(QUARK_POS, bc_compare_string("aaa", "aaa"), 0);
}
QUARK_TEST("bc_compare_string()", "", "", ""){
	ut_verify_auto(QUARK_POS, bc_compare_string("b", "a"), 1);
}


static int bc_compare_struct_true_deep(const types_t& types, const std::vector<rt_value_t>& left, const std::vector<rt_value_t>& right, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& struct_def = peek2(types, type).get_struct(types);

	for(int i = 0 ; i < struct_def._members.size() ; i++){
		const auto& member_type = struct_def._members[i]._type;
		int diff = compare_value_true_deep(types, left[i], right[i], member_type);
		if(diff != 0){
			return diff;
		}
	}
	return 0;
}

static int bc_compare_vectors_obj(const types_t& types, const immer::vector<bc_external_handle_t>& left, const immer::vector<bc_external_handle_t>& right, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto& peek = peek2(types, type);
	QUARK_ASSERT(peek.is_vector());

	const auto& shared_count = std::min(left.size(), right.size());
	const auto& element_type = peek.get_vector_element_type(types);
	for(int i = 0 ; i < shared_count ; i++){
		const auto element_result = compare_value_true_deep(
			types,
			rt_value_t(element_type, left[i]),
			rt_value_t(element_type, right[i]),
			element_type
		);
		if(element_result != 0){
			return element_result;
		}
	}
	if(left.size() == right.size()){
		return 0;
	}
	else if(left.size() > right.size()){
		return -1;
	}
	else{
		return +1;
	}
}

static int compare_bools(const bc_inplace_value_t& left, const bc_inplace_value_t& right){
	auto left2 = left.bool_value ? 1 : 0;
	auto right2 = right.bool_value ? 1 : 0;
	if(left2 < right2){
		return -1;
	}
	else if(left2 > right2){
		return 1;
	}
	else{
		return 0;
	}
}

static int compare_ints(const bc_inplace_value_t& left, const bc_inplace_value_t& right){
	if(left.int64_value < right.int64_value){
		return -1;
	}
	else if(left.int64_value > right.int64_value){
		return 1;
	}
	else{
		return 0;
	}
}
static int compare_doubles(const bc_inplace_value_t& left, const bc_inplace_value_t& right){
	if(left.double_value < right.double_value){
		return -1;
	}
	else if(left.double_value > right.double_value){
		return 1;
	}
	else{
		return 0;
	}
}

static int bc_compare_vectors_bool(const immer::vector<bc_inplace_value_t>& left, const immer::vector<bc_inplace_value_t>& right){
	const auto& shared_count = std::min(left.size(), right.size());
	for(int i = 0 ; i < shared_count ; i++){
		int result = compare_bools(left[i], right[i]);
		if(result != 0){
			return result;
		}
	}
	if(left.size() == right.size()){
		return 0;
	}
	else if(left.size() > right.size()){
		return -1;
	}
	else{
		return +1;
	}
}
static int bc_compare_vectors_int(const immer::vector<bc_inplace_value_t>& left, const immer::vector<bc_inplace_value_t>& right){
	const auto& shared_count = std::min(left.size(), right.size());
	for(int i = 0 ; i < shared_count ; i++){
		int result = compare_ints(left[i], right[i]);
		if(result != 0){
			return result;
		}
	}
	if(left.size() == right.size()){
		return 0;
	}
	else if(left.size() > right.size()){
		return -1;
	}
	else{
		return +1;
	}
}
static int bc_compare_vectors_double(const immer::vector<bc_inplace_value_t>& left, const immer::vector<bc_inplace_value_t>& right){
	const auto& shared_count = std::min(left.size(), right.size());
	for(int i = 0 ; i < shared_count ; i++){
		int result = compare_doubles(left[i], right[i]);
		if(result != 0){
			return result;
		}
	}
	if(left.size() == right.size()){
		return 0;
	}
	else if(left.size() > right.size()){
		return -1;
	}
	else{
		return +1;
	}
}



template <typename Map> bool bc_map_compare(Map const &lhs, Map const &rhs) {
	// No predicate needed because there is operator== for pairs already.
	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

static int bc_compare_dicts_obj(const types_t& types, const immer::map<std::string, bc_external_handle_t>& left, const immer::map<std::string, bc_external_handle_t>& right, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto peek = peek2(types, type);
	const auto& element_type = peek.get_dict_value_type(types);

	auto left_it = left.begin();
	auto left_end_it = left.end();

	auto right_it = right.begin();
	auto right_end_it = right.end();

	while(left_it != left_end_it && right_it != right_end_it){
		auto left_key = (*left_it).first;
		auto right_key = (*right_it).first;

		const auto key_result = bc_compare_string(left_key, right_key);
		if(key_result != 0){
			return key_result;
		}

		const auto element_result = compare_value_true_deep(
			types,
			rt_value_t(element_type, (*left_it).second),
			rt_value_t(element_type, (*right_it).second),
			element_type
		);
		if(element_result != 0){
			return element_result;
		}

		left_it++;
		right_it++;
	}

	if(left_it == left_end_it && right_it == right_end_it){
		return 0;
	}
	else if(left_it == left_end_it && right_it != right_end_it){
		return 1;
	}
	else if(left_it != left_end_it && right_it == right_end_it){
		return -1;
	}
	QUARK_ASSERT(false)
	quark::throw_exception();
}

//??? make template.
static int bc_compare_dicts_bool(const immer::map<std::string, bc_inplace_value_t>& left, const immer::map<std::string, bc_inplace_value_t>& right){
	auto left_it = left.begin();
	auto left_end_it = left.end();

	auto right_it = right.begin();
	auto right_end_it = right.end();

	while(left_it != left_end_it && right_it != right_end_it){
		auto left_key = (*left_it).first;
		auto right_key = (*right_it).first;

		const auto key_result = bc_compare_string(left_key, right_key);
		if(key_result != 0){
			return key_result;
		}

		int result = compare_bools((*left_it).second, (*right_it).second);
		if(result != 0){
			return result;
		}

		left_it++;
		right_it++;
	}

	if(left_it == left_end_it && right_it == right_end_it){
		return 0;
	}
	else if(left_it == left_end_it && right_it != right_end_it){
		return 1;
	}
	else if(left_it != left_end_it && right_it == right_end_it){
		return -1;
	}
	QUARK_ASSERT(false)
	quark::throw_exception();
}

static int bc_compare_dicts_int(const immer::map<std::string, bc_inplace_value_t>& left, const immer::map<std::string, bc_inplace_value_t>& right){
	auto left_it = left.begin();
	auto left_end_it = left.end();

	auto right_it = right.begin();
	auto right_end_it = right.end();

	while(left_it != left_end_it && right_it != right_end_it){
		auto left_key = (*left_it).first;
		auto right_key = (*right_it).first;

		const auto key_result = bc_compare_string(left_key, right_key);
		if(key_result != 0){
			return key_result;
		}

		int result = compare_ints((*left_it).second, (*right_it).second);
		if(result != 0){
			return result;
		}

		left_it++;
		right_it++;
	}

	if(left_it == left_end_it && right_it == right_end_it){
		return 0;
	}
	else if(left_it == left_end_it && right_it != right_end_it){
		return 1;
	}
	else if(left_it != left_end_it && right_it == right_end_it){
		return -1;
	}
	QUARK_ASSERT(false)
	quark::throw_exception();
}

static int bc_compare_dicts_double(const immer::map<std::string, bc_inplace_value_t>& left, const immer::map<std::string, bc_inplace_value_t>& right){
	auto left_it = left.begin();
	auto left_end_it = left.end();

	auto right_it = right.begin();
	auto right_end_it = right.end();

	while(left_it != left_end_it && right_it != right_end_it){
		auto left_key = (*left_it).first;
		auto right_key = (*right_it).first;

		const auto key_result = bc_compare_string(left_key, right_key);
		if(key_result != 0){
			return key_result;
		}

		int result = compare_doubles((*left_it).second, (*right_it).second);
		if(result != 0){
			return result;
		}

		left_it++;
		right_it++;
	}

	if(left_it == left_end_it && right_it == right_end_it){
		return 0;
	}
	else if(left_it == left_end_it && right_it != right_end_it){
		return 1;
	}
	else if(left_it != left_end_it && right_it == right_end_it){
		return -1;
	}
	QUARK_ASSERT(false)
	quark::throw_exception();
}

static int bc_compare_jsons(const json_t& lhs, const json_t& rhs){
	if(lhs == rhs){
		return 0;
	}
	else{
		const auto lhs_type = get_json_type(lhs);
		const auto rhs_type = get_json_type(rhs);
		int type_diff = rhs_type - lhs_type;
		if(type_diff != 0){
			return type_diff;
		}
		else{
			if(lhs.is_object()){
				//??? NOT IMPLEMENTED YET
				QUARK_ASSERT(false);
				quark::throw_exception();
			}
			else if(lhs.is_array()){
				//??? NOT IMPLEMENTED YET
				QUARK_ASSERT(false);
				quark::throw_exception();
			}
			else if(lhs.is_string()){
				const auto diff = std::strcmp(lhs.get_string().c_str(), rhs.get_string().c_str());
				return diff < 0 ? -1 : 1;
			}
			else if(lhs.is_number()){
				const auto lhs_number = lhs.get_number();
				const auto rhs_number = rhs.get_number();
				return lhs_number < rhs_number ? -1 : 1;
			}
			else if(lhs.is_true()){
				return 0;
			}
			else if(lhs.is_false()){
				return 0;
			}
			else if(lhs.is_null()){
				return 0;
			}
			else{
				QUARK_ASSERT(false);
				quark::throw_exception();
			}
		}
	}
}

static int bc_compare_value_exts(const types_t& types, const bc_external_handle_t& left, const bc_external_handle_t& right, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(left.check_invariant());
	QUARK_ASSERT(right.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	return compare_value_true_deep(types, rt_value_t(type, left), rt_value_t(type, right), type);
}

int compare_value_true_deep(value_backend_t& backend, const rt_value_t& left, const rt_value_t& right, const type_t& type0){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(left._type == right._type);
	QUARK_ASSERT(left.check_invariant());
	QUARK_ASSERT(right.check_invariant());

	auto& types = backend.types;

	const auto type = type0;
	const auto peek = peek2(types, type);

	if(type.is_undefined()){
		return 0;
	}
	else if(peek.is_bool()){
		return (left.get_bool_value() ? 1 : 0) - (right.get_bool_value() ? 1 : 0);
	}
	else if(peek.is_int()){
		return compare(left.get_int_value() - right.get_int_value());
	}
	else if(peek.is_double()){
		const auto a = left.get_double_value();
		const auto b = right.get_double_value();
		if(a > b){
			return 1;
		}
		else if(a < b){
			return -1;
		}
		else{
			return 0;
		}
	}
	else if(peek.is_string()){
		return bc_compare_string(left.get_string_value(), right.get_string_value());
	}
	else if(peek.is_json()){
		return bc_compare_jsons(left.get_json(), right.get_json());
	}
	else if(peek.is_typeid()){
		if(left.get_typeid_value() == right.get_typeid_value()){
			return 0;
		}
		else{
			return -1;//??? Hack -- should return +1 depending on values.
		}
	}
	else if(peek.is_struct()){
		//	Make sure the EXACT struct types are the same -- not only that they are both structs
		return bc_compare_struct_true_deep(types, left.get_struct_value(), right.get_struct_value(), type0);
	}
	else if(peek.is_vector()){
		const auto element_type_peek = peek2(types, peek.get_vector_element_type(types));
		if(false){
		}
		else if(element_type_peek.is_bool()){
			return bc_compare_vectors_bool(left._pod._external->_vector_w_inplace_elements, right._pod._external->_vector_w_inplace_elements);
		}
		else if(element_type_peek.is_int()){
			return bc_compare_vectors_int(left._pod._external->_vector_w_inplace_elements, right._pod._external->_vector_w_inplace_elements);
		}
		else if(element_type_peek.is_double()){
			return bc_compare_vectors_double(left._pod._external->_vector_w_inplace_elements, right._pod._external->_vector_w_inplace_elements);
		}
		else{
			const auto& left_vec = get_vector_external_elements(types, left);
			const auto& right_vec = get_vector_external_elements(types, right);
			return bc_compare_vectors_obj(types, *left_vec, *right_vec, type0);
		}
	}
	else if(peek.is_dict()){
		const auto dict_value_peek = peek2(types, peek.get_dict_value_type(types));
		if(false){
		}
		else if(dict_value_peek.is_bool()){
			return bc_compare_dicts_bool(left._pod._external->_dict_w_inplace_values, right._pod._external->_dict_w_inplace_values);
		}
		else if(dict_value_peek.is_int()){
			return bc_compare_dicts_int(left._pod._external->_dict_w_inplace_values, right._pod._external->_dict_w_inplace_values);
		}
		else if(dict_value_peek.is_double()){
			return bc_compare_dicts_double(left._pod._external->_dict_w_inplace_values, right._pod._external->_dict_w_inplace_values);
		}
		else  {
			QUARK_ASSERT(encode_as_dict_w_inplace_values(types, type) == false);

			const auto& left2 = get_dict_external_values(types, left);
			const auto& right2 = get_dict_external_values(types, right);
			return bc_compare_dicts_obj(types, left2, right2, type0);
		}
	}
	else if(peek2(types, type).is_function()){
		QUARK_ASSERT(false);
		return 0;
	}
	else{
		QUARK_ASSERT(false);
		quark::throw_exception();
	}
}
#endif









int compare_values(value_backend_t& backend, int64_t op, const runtime_type_t type, runtime_value_t lhs, runtime_value_t rhs){
	QUARK_ASSERT(backend.check_invariant());

	const auto& value_type = lookup_type_ref(backend, type);

	const auto left_value = from_runtime_value2(backend, lhs, value_type);
	const auto right_value = from_runtime_value2(backend, rhs, value_type);

	const int result = value_t::compare_value_true_deep(backend.types, left_value, right_value);
//	int result = runtime_compare_value_true_deep((const uint64_t)lhs, (const uint64_t)rhs, vector_type);
	const auto op2 = static_cast<expression_type>(op);
	if(op2 == expression_type::k_comparison_smaller_or_equal){
		return result <= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_smaller){
		return result < 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger_or_equal){
		return result >= 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_comparison_larger){
		return result > 0 ? 1 : 0;
	}

	else if(op2 == expression_type::k_logical_equal){
		return result == 0 ? 1 : 0;
	}
	else if(op2 == expression_type::k_logical_nonequal){
		return result != 0 ? 1 : 0;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

runtime_value_t update_element__vector(value_backend_t& backend, runtime_value_t obj1, runtime_type_t coll_type, runtime_value_t index, runtime_value_t value){
	QUARK_ASSERT(backend.check_invariant());

	const auto& types = backend.types;
	const auto vector_type = type_t(coll_type);

	const auto& obj1_peek = peek2(types, vector_type);
	const auto element_type = obj1_peek.get_vector_element_type(types);
	const bool element_is_rc = is_rc_value(backend.types, peek2(types, element_type));

	const auto backend_mode = backend.config.vector_backend_mode;
	if(backend_mode == vector_backend::carray){
		return update_element__vector_carray(backend, obj1, coll_type, index, value);
	}
	else if(backend_mode == vector_backend::hamt){
		if(element_is_rc){
			return update_element__vector_hamt_nonpod(backend, obj1, coll_type, index, value);
		}
		else{
			return update_element__vector_hamt_pod(backend, obj1, coll_type, index, value);
		}
	}
	else{
		UNSUPPORTED();
	}
}

runtime_value_t update_dict(value_backend_t& backend, runtime_value_t obj1, runtime_type_t coll_type, runtime_value_t key_value, runtime_value_t value){
	QUARK_ASSERT(backend.check_invariant());

	const auto backend_mode = backend.config.dict_backend_mode;
	if(backend_mode == dict_backend::cppmap){
		return update__dict_cppmap(backend, obj1, coll_type, key_value, value);
	}
	else if(backend_mode == dict_backend::hamt){
		return update__dict_hamt(backend, obj1, coll_type, key_value, value);
	}
	else{
		UNSUPPORTED();
	}
}





const runtime_value_t update_element__vector_carray(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t index, runtime_value_t value){
	QUARK_ASSERT(backend.check_invariant());

	const auto vec = unpack_vector_carray_arg(backend, coll_value, coll_type);
	const auto index2 = index.int_value;
	const auto element_itype = lookup_vector_element_type(backend, type_t(coll_type));

	if(index2 < 0 || index2 >= vec->get_element_count()){
		quark::throw_runtime_error("Position argument to update() is outside collection span.");
	}

	auto result = alloc_vector_carray(backend.heap, vec->get_element_count(), vec->get_element_count(), type_t(coll_type));
	auto dest_ptr = result.vector_carray_ptr->get_element_ptr();
	auto source_ptr = vec->get_element_ptr();
	if(is_rc_value(backend.types, element_itype)){
		retain_value(backend, value, element_itype);
		for(int i = 0 ; i < result.vector_carray_ptr->get_element_count() ; i++){
			retain_value(backend, source_ptr[i], element_itype);
			dest_ptr[i] = source_ptr[i];
		}

		if(is_rc_value(backend.types, type_t(coll_type))){
			release_value(backend, dest_ptr[index2], type_t(coll_type));
		}
		dest_ptr[index2] = value;
	}
	else{
		for(int i = 0 ; i < result.vector_carray_ptr->get_element_count() ; i++){
			dest_ptr[i] = source_ptr[i];
		}
		dest_ptr[index2] = value;
	}

	return result;
}


const runtime_value_t update__dict_cppmap(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_value_t value){
	QUARK_ASSERT(backend.check_invariant());

	//??? move to compile time
	const auto& type0 = lookup_type_ref(backend, coll_type);

	const auto key = from_runtime_string2(backend, key_value);
	const auto dict = unpack_dict_cppmap_arg(backend, coll_value, coll_type);

	//??? compile time
	const auto value_itype = peek2(backend.types, type0).get_dict_value_type(backend.types);

	//	Deep copy dict.
	auto dict2 = alloc_dict_cppmap(backend.heap, type_t(coll_type));
	dict2.dict_cppmap_ptr->get_map_mut() = dict->get_map();

	dict2.dict_cppmap_ptr->get_map_mut().insert_or_assign(key, value);

	if(is_rc_value(backend.types, value_itype)){
		for(const auto& e: dict2.dict_cppmap_ptr->get_map()){
			retain_value(backend, e.second, value_itype);
		}
	}

	return dict2;
}

const runtime_value_t update__dict_hamt(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_value_t value){
	QUARK_ASSERT(backend.check_invariant());

	//??? move to compile time
	const auto& type0 = lookup_type_ref(backend, coll_type);

	const auto key = from_runtime_string2(backend, key_value);
	const auto dict = coll_value.dict_hamt_ptr;

	//??? compile time
	const auto value_itype = peek2(backend.types, type0).get_dict_value_type(backend.types);

	//	Deep copy dict.
	auto dict2 = alloc_dict_hamt(backend.heap, type_t(coll_type));
	dict2.dict_hamt_ptr->get_map_mut() = dict->get_map();

	dict2.dict_hamt_ptr->get_map_mut() = dict2.dict_hamt_ptr->get_map_mut().set(key, value);

	if(is_rc_value(backend.types, value_itype)){
		for(const auto& e: dict2.dict_hamt_ptr->get_map()){
			retain_value(backend, e.second, value_itype);
		}
	}

	return dict2;
}





/*
//??? The update mechanism uses strings == slow.
static rt_value_t update_struct_member_shallow(interpreter_t& vm, const rt_value_t& obj, const std::string& member_name, const rt_value_t& new_value){
	QUARK_ASSERT(obj.check_invariant());
	const auto& types = vm._imm->_program._types;
	QUARK_ASSERT(peek2(types, obj._type).is_struct());
	QUARK_ASSERT(member_name.empty() == false);
	QUARK_ASSERT(new_value.check_invariant());

	const auto& values = obj.get_struct_value(vm._backend);
	const auto& struct_def = peek2(types, obj._type).get_struct(types);

	int member_index = find_struct_member_index(struct_def, member_name);
	if(member_index == -1){
		quark::throw_runtime_error("Unknown member.");
	}

#if DEBUG && 0
	QUARK_TRACE(typeid_to_compact_string(new_value._type));
	QUARK_TRACE(typeid_to_compact_string(struct_def._members[member_index]._type));

	const auto dest_member_entry = struct_def._members[member_index];
#endif

	auto values2 = values;
	values2[member_index] = new_value;

	auto s2 = rt_value_t::make_struct_value(vm._backend, obj._type, values2);
	return s2;
}

static rt_value_t update_struct_member_deep(interpreter_t& vm, const rt_value_t& obj, const std::vector<std::string>& path, const rt_value_t& new_value){
	QUARK_ASSERT(obj.check_invariant());
	QUARK_ASSERT(path.empty() == false);
	QUARK_ASSERT(new_value.check_invariant());

	if(path.size() == 1){
		return update_struct_member_shallow(vm, obj, path[0], new_value);
	}
	else{
		const auto& types = vm._imm->_program._types;

		std::vector<std::string> subpath = path;
		subpath.erase(subpath.begin());

		const auto& values = obj.get_struct_value(vm._backend);
		const auto& struct_def = peek2(types, obj._type).get_struct(types);
		int member_index = find_struct_member_index(struct_def, path[0]);
		if(member_index == -1){
			quark::throw_runtime_error("Unknown member.");
		}

		const auto& child_value = values[member_index];
		const auto& child_type = struct_def._members[member_index]._type;
		if(peek2(types, child_type).is_struct() == false){
			quark::throw_runtime_error("Value type not matching struct member type.");
		}

		const auto child2 = update_struct_member_deep(vm, child_value, subpath, new_value);
		const auto obj2 = update_struct_member_shallow(vm, obj, path[0], child2);
		return obj2;
	}
}
*/

//??? Slow
runtime_value_t update_struct_member(value_backend_t& backend, runtime_value_t struct_value, const type_t& struct_type0, int member_index, runtime_value_t value, const type_t& member_type){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(peek2(backend.types, struct_type0).is_struct());
	QUARK_ASSERT(value.check_invariant());

	QUARK_ASSERT(struct_value.struct_ptr->check_invariant());

	const auto& struct_type_peek = peek2(backend.types, struct_type0);
	const auto& value2 = rt_value_t(backend, peek2(backend.types, member_type), value, rt_value_t::rc_mode::bump);
	const auto& values = from_runtime_struct(backend, struct_value, struct_type_peek);

	auto values2 = values;
	values2[member_index] = value2;

	auto result = rt_value_t::make_struct_value(backend, struct_type0, values2);
	retain_value(backend, result._pod, struct_type0);
	return result._pod;
}









const runtime_value_t subset__string(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, uint64_t start, uint64_t end){
	QUARK_ASSERT(backend.check_invariant());

	if(start < 0 || end < 0){
		quark::throw_runtime_error("subset() requires start and end to be non-negative.");
	}

	const auto value = from_runtime_string2(backend, coll_value);
	const auto len = get_vec_string_size(coll_value);
	const auto end2 = std::min(end, len);
	const auto start2 = std::min(start, end2);
	const auto len2 = end2 - start2;

	const auto s = std::string(&value[start2], &value[start2 + len2]);
	const auto result = to_runtime_string2(backend, s);
	return result;
}

const runtime_value_t subset__carray(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, uint64_t start, uint64_t end){
	QUARK_ASSERT(backend.check_invariant());

	if(start < 0 || end < 0){
		quark::throw_runtime_error("subset() requires start and end to be non-negative.");
	}

	const auto& type0 = lookup_type_ref(backend, coll_type);
	const auto vec = unpack_vector_carray_arg(backend, coll_value, coll_type);
	const auto end2 = std::min(end, vec->get_element_count());
	const auto start2 = std::min(start, end2);
	const auto len2 = end2 - start2;
	if(len2 >= INT64_MAX){
		throw std::exception();
	}

	const auto element_itype = lookup_vector_element_type(backend, type_t(coll_type));

	auto vec2 = alloc_vector_carray(backend.heap, len2, len2, type0);
	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < len2 ; i++){
			const auto& value = vec->get_element_ptr()[start2 + i];
			vec2.vector_carray_ptr->get_element_ptr()[i] = value;
			retain_value(backend, value, element_itype);
		}
	}
	else{
		for(int i = 0 ; i < len2 ; i++){
			const auto& value = vec->get_element_ptr()[start2 + i];
			vec2.vector_carray_ptr->get_element_ptr()[i] = value;
		}
	}
	return vec2;
}

const runtime_value_t subset__hamt(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, uint64_t start, uint64_t end){
	QUARK_ASSERT(backend.check_invariant());

	if(start < 0 || end < 0){
		quark::throw_runtime_error("subset() requires start and end to be non-negative.");
	}

	const auto& vec = *coll_value.vector_hamt_ptr;
	const auto end2 = std::min(end, vec.get_element_count());
	const auto start2 = std::min(start, end2);
	const auto len2 = end2 - start2;
	if(len2 >= INT64_MAX){
		throw std::exception();
	}

	const auto element_itype = lookup_vector_element_type(backend, type_t(coll_type));

	auto vec2 = alloc_vector_hamt(backend.heap, len2, len2, type_t(coll_type));
	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < len2 ; i++){
			const auto& value = vec.load_element(start2 + i);
			vec2.vector_hamt_ptr->store_mutate(i, value);
			retain_value(backend, value, element_itype);
		}
	}
	else{
		for(int i = 0 ; i < len2 ; i++){
			const auto& value = vec.load_element(start2 + i);
			vec2.vector_hamt_ptr->store_mutate(i, value);
		}
	}
	return vec2;
}


//	assert(subset("abc", 1, 3) == "bc");
runtime_value_t subset(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, int64_t start, int64_t end){
	QUARK_ASSERT(backend.check_invariant());


	if(start < 0 || end < 0){
		quark::throw_runtime_error("subset() requires start and end to be non-negative.");
	}

	const auto obj_type = type_t(coll_type);
	if(obj_type.is_string()){
		return subset__string(backend, coll_value, coll_type, start, end);
	}
	else if(is_vector_carray(backend.types, backend.config, obj_type)){
		return subset__carray(backend, coll_value, coll_type, start, end);
	}
	else if(is_vector_hamt(backend.types, backend.config, obj_type)){
		return subset__hamt(backend, coll_value, coll_type, start, end);
	}
	else{
		quark::throw_runtime_error("Calling push_back() on unsupported type of value.");
	}
}








static void check_replace_indexes(std::size_t start, std::size_t end){
	if(start < 0 || end < 0){
		quark::throw_runtime_error("replace() requires start and end to be non-negative.");
	}
	if(start > end){
		quark::throw_runtime_error("replace() requires start <= end.");
	}
}

runtime_value_t replace__string(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, size_t start, size_t end, runtime_value_t replacement_value, runtime_type_t replacement_type){
	QUARK_ASSERT(backend.check_invariant());

	check_replace_indexes(start, end);
	const auto& type0 = lookup_type_ref(backend, coll_type);
	const auto& type3 = lookup_type_ref(backend, replacement_type);

	QUARK_ASSERT(type3 == type0);

	const auto s = from_runtime_string2(backend, coll_value);
	const auto replace = from_runtime_string2(backend, replacement_value);

	auto s_len = s.size();
	auto replace_len = replace.size();

	auto end2 = std::min(end, s_len);
	auto start2 = std::min(start, end2);

	const std::size_t len2 = start2 + replace_len + (s_len - end2);
	auto s2 = reinterpret_cast<char*>(std::malloc(len2 + 1));
	std::memcpy(&s2[0], &s[0], start2);
	std::memcpy(&s2[start2], &replace[0], replace_len);
	std::memcpy(&s2[start2 + replace_len], &s[end2], s_len - end2);

	const std::string ret(s2, &s2[start2 + replace_len + (s_len - end2)]);
	const auto result2 = to_runtime_string2(backend, ret);
	return result2;
}

runtime_value_t replace__carray(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, size_t start, size_t end, runtime_value_t replacement_value, runtime_type_t replacement_type){
	QUARK_ASSERT(backend.check_invariant());

	check_replace_indexes(start, end);

	const auto& type0 = lookup_type_ref(backend, coll_type);
	QUARK_ASSERT(lookup_type_ref(backend, replacement_type) == type0);

	const auto element_itype = lookup_vector_element_type(backend, type_t(coll_type));

	const auto vec = unpack_vector_carray_arg(backend, coll_value, coll_type);
	const auto replace_vec = unpack_vector_carray_arg(backend, replacement_value, replacement_type);

	auto end2 = std::min(end, (size_t)vec->get_element_count());
	auto start2 = std::min(start, end2);

	const auto section1_len = start2;
	const auto section2_len = replace_vec->get_element_count();
	const auto section3_len = vec->get_element_count() - end2;

	const auto len2 = section1_len + section2_len + section3_len;
	auto vec2 = alloc_vector_carray(backend.heap, len2, len2, type0);
	copy_elements(&vec2.vector_carray_ptr->get_element_ptr()[0], &vec->get_element_ptr()[0], section1_len);
	copy_elements(&vec2.vector_carray_ptr->get_element_ptr()[section1_len], &replace_vec->get_element_ptr()[0], section2_len);
	copy_elements(&vec2.vector_carray_ptr->get_element_ptr()[section1_len + section2_len], &vec->get_element_ptr()[end2], section3_len);

	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < len2 ; i++){
			retain_value(backend, vec2.vector_carray_ptr->get_element_ptr()[i], element_itype);
		}
	}

	return vec2;
}
runtime_value_t replace__hamt(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, size_t start, size_t end, runtime_value_t replacement_value, runtime_type_t replacement_type){
	QUARK_ASSERT(backend.check_invariant());

	check_replace_indexes(start, end);

	const auto& type0 = lookup_type_ref(backend, coll_type);
	QUARK_ASSERT(lookup_type_ref(backend, replacement_type) == type0);

	const auto element_itype = lookup_vector_element_type(backend, type_t(coll_type));

	const auto& vec = *coll_value.vector_hamt_ptr;
	const auto& replace_vec = *replacement_value.vector_hamt_ptr;

	auto end2 = std::min(end, (size_t)vec.get_element_count());
	auto start2 = std::min(start, end2);

	const auto section1_len = start2;
	const auto section2_len = replace_vec.get_element_count();
	const auto section3_len = vec.get_element_count() - end2;

	const auto len2 = section1_len + section2_len + section3_len;
	auto vec2 = alloc_vector_hamt(backend.heap, len2, len2, type_t(coll_type));
	for(size_t i = 0 ; i < section1_len ; i++){
		const auto& value = vec.load_element(0 + i);
		vec2.vector_hamt_ptr->store_mutate(0 + i, value);
	}
	for(size_t i = 0 ; i < section2_len ; i++){
		const auto& value = replace_vec.load_element(0 + i);
		vec2.vector_hamt_ptr->store_mutate(section1_len + i, value);
	}
	for(size_t i = 0 ; i < section3_len ; i++){
		const auto& value = vec.load_element(end2 + i);
		vec2.vector_hamt_ptr->store_mutate(section1_len + section2_len + i, value);
	}

	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < len2 ; i++){
			retain_value(backend, vec2.vector_hamt_ptr->load_element(i), element_itype);
		}
	}

	return vec2;
}





//	assert(replace("One ring to rule them all", 4, 7, "rabbit") == "One rabbit to rule them all");
runtime_value_t replace(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, int64_t start, int64_t end, runtime_value_t replacement_value, runtime_type_t replacement_type){
	QUARK_ASSERT(backend.check_invariant());

	if(start < 0 || end < 0){
		quark::throw_runtime_error("replace() requires start and end to be non-negative.");
	}
	if(start > end){
		quark::throw_runtime_error("replace() requires start <= end.");
	}
	QUARK_ASSERT(coll_type == replacement_type);

	const auto obj_type = type_t(coll_type);
	if(obj_type.is_string()){
		return replace__string(backend, coll_value, coll_type, start, end, replacement_value, replacement_type);
	}
	else if(is_vector_carray(backend.types, backend.config, obj_type)){
		return replace__carray(backend, coll_value, coll_type, start, end, replacement_value, replacement_type);
	}
	else if(is_vector_hamt(backend.types, backend.config, obj_type)){
		return replace__hamt(backend, coll_value, coll_type, start, end, replacement_value, replacement_type);
	}
	else{
		quark::throw_runtime_error("Calling push_back() on unsupported type of value.");
	}
}







int64_t find__string(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, const runtime_value_t value, runtime_type_t value_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type1 = lookup_type_ref(backend, value_type);

	QUARK_ASSERT(peek2(backend.types, type1).is_string());

	const auto str = from_runtime_string2(backend, coll_value);
	const auto wanted2 = from_runtime_string2(backend, value);
	const auto pos = str.find(wanted2);
	const auto result = pos == std::string::npos ? -1 : static_cast<int64_t>(pos);
	return result;
}
int64_t find__carray(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, const runtime_value_t value, runtime_type_t value_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, coll_type);
	const auto& type1 = lookup_type_ref(backend, value_type);

	QUARK_ASSERT(type1 == peek2(backend.types, type0).get_vector_element_type(backend.types));

	const auto vec = unpack_vector_carray_arg(backend, coll_value, coll_type);

//		auto it = std::find_if(function_defs.begin(), function_defs.end(), [&] (const function_def_t& e) { return e.def_name == function_name; } );
	const auto it = std::find_if(
		vec->get_element_ptr(),
		vec->get_element_ptr() + vec->get_element_count(),
		[&] (const runtime_value_t& e) {
			return compare_values(backend, static_cast<int64_t>(expression_type::k_logical_equal), value_type, e, value) == 1;
		}
	);
	if(it == vec->get_element_ptr() + vec->get_element_count()){
		return -1;
	}
	else{
		const auto pos = it - vec->get_element_ptr();
		return pos;
	}
}
int64_t find__hamt(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, const runtime_value_t value, runtime_type_t value_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, coll_type);
	const auto& type1 = lookup_type_ref(backend, value_type);

	QUARK_ASSERT(type1 == peek2(backend.types, type0).get_vector_element_type(backend.types));

	const auto& vec = *coll_value.vector_hamt_ptr;
	QUARK_ASSERT(vec.check_invariant());

	const auto count = vec.get_element_count();
	int64_t index = 0;
	while(index < count && compare_values(backend, static_cast<int64_t>(expression_type::k_logical_equal), value_type, vec.load_element(index), value) != 1){
		index++;
	}
	if(index == count){
		return -1;
	}
	else{
		return index;
	}
}


int64_t find2(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, const runtime_value_t value, runtime_type_t value_type){
	const auto& type0 = lookup_type_ref(backend, coll_type);
	if(peek2(backend.types, type0).is_string()){
		return find__string(backend, coll_value, coll_type, value, value_type);
	}
	else if(is_vector_carray(backend.types, backend.config, type0)){
		return find__carray(backend, coll_value, coll_type, value, value_type);
	}
	else if(is_vector_hamt(backend.types, backend.config, type0)){
		return find__hamt(backend, coll_value, coll_type, value, value_type);
	}
	else{
		//	No other types allowed.
		UNSUPPORTED();
	}
}

//??? use rt_value_t as primitive for all features.

bool exists(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t value, runtime_type_t value_type){
	const auto& types = backend.types;
	const auto& type0 = lookup_type_ref(backend, coll_type);
//	const auto& type1 = lookup_type_ref(backend, value_type);
	QUARK_ASSERT(peek2(types, type0).is_dict());

	if(is_dict_cppmap(types, backend.config, type0)){
		const auto& dict = unpack_dict_cppmap_arg(backend, coll_value, coll_type);
		const auto key_string = from_runtime_string2(backend, value);

		const auto& m = dict->get_map();
		const auto it = m.find(key_string);
		return it != m.end() ? 1 : 0;
	}
	else if(is_dict_hamt(types, backend.config, type0)){
		const auto& dict = *coll_value.dict_hamt_ptr;
		const auto key_string = from_runtime_string2(backend, value);

		const auto& m = dict.get_map();
		const auto it = m.find(key_string);
		return it != nullptr ? 1 : 0;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

runtime_value_t erase(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type, runtime_value_t key_value, runtime_type_t key_type){
	const auto& types = backend.types;
	const auto& type0 = lookup_type_ref(backend, coll_type);
	const auto& type1 = lookup_type_ref(backend, key_type);

	QUARK_ASSERT(peek2(types, type0).is_dict());
	QUARK_ASSERT(peek2(types, type1).is_string());

	if(is_dict_cppmap(types, backend.config, type0)){
		const auto& dict = unpack_dict_cppmap_arg(backend, coll_value, coll_type);

		const auto value_type = peek2(types, type0).get_dict_value_type(types);

		//	Deep copy dict.
		auto dict2 = alloc_dict_cppmap(backend.heap, type0);
		auto& m = dict2.dict_cppmap_ptr->get_map_mut();
		m = dict->get_map();

		const auto key_string = from_runtime_string2(backend, key_value);
		m.erase(key_string);

		if(is_rc_value(types, value_type)){
			for(auto& e: m){
				retain_value(backend, e.second, value_type);
			}
		}
		return dict2;
	}
	else if(is_dict_hamt(types, backend.config, type0)){
		const auto& dict = *coll_value.dict_hamt_ptr;

		const auto value_type = peek2(types, type0).get_dict_value_type(types);

		//	Deep copy dict.
		auto dict2 = alloc_dict_hamt(backend.heap, type0);
		auto& m = dict2.dict_hamt_ptr->get_map_mut();
		m = dict.get_map();

		const auto key_string = from_runtime_string2(backend, key_value);
		m = m.erase(key_string);

		if(is_rc_value(types, value_type)){
			for(auto& e: m){
				retain_value(backend, e.second, value_type);
			}
		}
		return dict2;
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

//??? optimize prio 1
//??? We need to figure out the return type *again*, knowledge we have already in semast.
runtime_value_t get_keys(value_backend_t& backend, runtime_value_t coll_value, runtime_type_t coll_type){
	const auto& types = backend.types;
	const auto& type0 = lookup_type_ref(backend, coll_type);
	QUARK_ASSERT(peek2(types, type0).is_dict());

	if(is_dict_cppmap(types, backend.config, type0)){
		if(backend.config.vector_backend_mode == vector_backend::carray){
			return get_keys__cppmap_carray(backend, coll_value, coll_type);
		}
		else if(backend.config.vector_backend_mode == vector_backend::hamt){
			return get_keys__cppmap_hamt(backend, coll_value, coll_type);
		}
		else{
			QUARK_ASSERT(false);
			throw std::exception();
		}
	}
	else if(is_dict_hamt(types, backend.config, type0)){
		if(backend.config.vector_backend_mode == vector_backend::carray){
			return get_keys__hamtmap_carray(backend, coll_value, coll_type);
		}
		else if(backend.config.vector_backend_mode == vector_backend::hamt){
			return get_keys__hamtmap_hamt(backend, coll_value, coll_type);
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





runtime_value_t get_keys__cppmap_carray(value_backend_t& backend, runtime_value_t dict_value, runtime_type_t dict_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, dict_type);

	QUARK_ASSERT(peek2(backend.types, type0).is_dict());

	const auto& dict = unpack_dict_cppmap_arg(backend, dict_value, dict_type);
	const auto& m = dict->get_map();
	const auto count = (uint64_t)m.size();

	auto result_vec = alloc_vector_carray(backend.heap, count, count, type_t::make_vector(backend.types, type_t::make_string()));

	int index = 0;
	for(const auto& e: m){
		//	Notice that the internal representation of dictionary keys are std::string, not floyd-strings,
		//	so we need to create new key-strings from scratch.
		const auto key = to_runtime_string2(backend, e.first);
		result_vec.vector_carray_ptr->get_element_ptr()[index] = key;
		index++;
	}
	return result_vec;
}
runtime_value_t get_keys__cppmap_hamt(value_backend_t& backend, runtime_value_t dict_value, runtime_type_t dict_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, dict_type);

	QUARK_ASSERT(peek2(backend.types, type0).is_dict());

	const auto& dict = unpack_dict_cppmap_arg(backend, dict_value, dict_type);
	const auto& m = dict->get_map();
	const auto count = (uint64_t)m.size();

	auto result_vec = alloc_vector_hamt(backend.heap, count, count, type_t::make_vector(backend.types, type_t::make_string()));

	int index = 0;
	for(const auto& e: m){
		//	Notice that the internal representation of dictionary keys are std::string, not floyd-strings,
		//	so we need to create new key-strings from scratch.
		const auto key = to_runtime_string2(backend, e.first);
		result_vec.vector_hamt_ptr->store_mutate(index, key);
		index++;
	}
	return result_vec;
}


runtime_value_t get_keys__hamtmap_carray(value_backend_t& backend, runtime_value_t dict_value, runtime_type_t dict_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, dict_type);

	QUARK_ASSERT(peek2(backend.types, type0).is_dict());

	const auto& dict = dict_value.dict_hamt_ptr;
	const auto& m = dict->get_map();
	const auto count = (uint64_t)m.size();

	auto result_vec = alloc_vector_carray(backend.heap, count, count, type_t::make_vector(backend.types, type_t::make_string()));

	int index = 0;
	for(const auto& e: m){
		//	Notice that the internal representation of dictionary keys are std::string, not floyd-strings,
		//	so we need to create new key-strings from scratch.
		const auto key = to_runtime_string2(backend, e.first);
		result_vec.vector_carray_ptr->get_element_ptr()[index] = key;
		index++;
	}
	return result_vec;
}
runtime_value_t get_keys__hamtmap_hamt(value_backend_t& backend, runtime_value_t dict_value, runtime_type_t dict_type){
	QUARK_ASSERT(backend.check_invariant());

	const auto& type0 = lookup_type_ref(backend, dict_type);

	QUARK_ASSERT(peek2(backend.types, type0).is_dict());

	const auto& dict = dict_value.dict_hamt_ptr;
	const auto& m = dict->get_map();
	const auto count = (uint64_t)m.size();

	auto result_vec = alloc_vector_hamt(backend.heap, count, count, type_t::make_vector(backend.types, type_t::make_string()));

	int index = 0;
	for(const auto& e: m){
		//	Notice that the internal representation of dictionary keys are std::string, not floyd-strings,
		//	so we need to create new key-strings from scratch.
		const auto key = to_runtime_string2(backend, e.first);
		result_vec.vector_hamt_ptr->store_mutate(index, key);
		index++;
	}
	return result_vec;
}












runtime_value_t concat_strings(value_backend_t& backend, const runtime_value_t& lhs, const runtime_value_t& rhs){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(lhs.check_invariant());
	QUARK_ASSERT(rhs.check_invariant());

	const auto result = from_runtime_string2(backend, lhs) + from_runtime_string2(backend, rhs);
	return to_runtime_string2(backend, result);
}

runtime_value_t concat_vector_carray(value_backend_t& backend, const type_t& type, const runtime_value_t& lhs, const runtime_value_t& rhs){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(lhs.check_invariant());
	QUARK_ASSERT(rhs.check_invariant());

	const auto count2 = lhs.vector_carray_ptr->get_element_count() + rhs.vector_carray_ptr->get_element_count();

	auto result = alloc_vector_carray(backend.heap, count2, count2, type);

	//??? warning: assumes element = allocation.

	const auto element_itype = lookup_vector_element_type(backend, type);

	auto dest_ptr = result.vector_carray_ptr->get_element_ptr();
	auto dest_ptr2 = dest_ptr + lhs.vector_carray_ptr->get_element_count();
	auto lhs_ptr = lhs.vector_carray_ptr->get_element_ptr();
	auto rhs_ptr = rhs.vector_carray_ptr->get_element_ptr();

	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < lhs.vector_carray_ptr->get_element_count() ; i++){
			retain_value(backend, lhs_ptr[i], element_itype);
			dest_ptr[i] = lhs_ptr[i];
		}
		for(int i = 0 ; i < rhs.vector_carray_ptr->get_element_count() ; i++){
			retain_value(backend, rhs_ptr[i], element_itype);
			dest_ptr2[i] = rhs_ptr[i];
		}
	}
	else{
		for(int i = 0 ; i < lhs.vector_carray_ptr->get_element_count() ; i++){
			dest_ptr[i] = lhs_ptr[i];
		}
		for(int i = 0 ; i < rhs.vector_carray_ptr->get_element_count() ; i++){
			dest_ptr2[i] = rhs_ptr[i];
		}
	}
	return result;
}

runtime_value_t concat_vector_hamt(value_backend_t& backend, const type_t& type, const runtime_value_t& lhs, const runtime_value_t& rhs){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(lhs.check_invariant());
	QUARK_ASSERT(rhs.check_invariant());

	const auto lhs_count = lhs.vector_hamt_ptr->get_element_count();
	const auto rhs_count = rhs.vector_hamt_ptr->get_element_count();

	const auto count2 = lhs_count + rhs_count;

	auto result = alloc_vector_hamt(backend.heap, count2, count2, type);

	//??? warning: assumes element = allocation.

	const auto element_itype = lookup_vector_element_type(backend, type);

	//??? Causes a full path copy for EACH ELEMENT = slow. better to make new hamt in one go.
	if(is_rc_value(backend.types, element_itype)){
		for(int i = 0 ; i < lhs_count ; i++){
			auto value = lhs.vector_hamt_ptr->load_element(i);
			retain_value(backend, value, element_itype);
			result.vector_hamt_ptr->store_mutate(i, value);
		}
		for(int i = 0 ; i < rhs_count ; i++){
			auto value = rhs.vector_hamt_ptr->load_element(i);
			retain_value(backend, value, element_itype);
			result.vector_hamt_ptr->store_mutate(lhs_count + i, value);
		}
	}
	else{
		for(int i = 0 ; i < lhs_count ; i++){
			auto value = lhs.vector_hamt_ptr->load_element(i);
			result.vector_hamt_ptr->store_mutate(i, value);
		}
		for(int i = 0 ; i < rhs_count ; i++){
			auto value = rhs.vector_hamt_ptr->load_element(i);
			result.vector_hamt_ptr->store_mutate(lhs_count + i, value);
		}
	}
	return result;
}


runtime_value_t concatunate_vectors(value_backend_t& backend, const type_t& type, runtime_value_t lhs, runtime_value_t rhs){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(type.check_invariant());
	QUARK_ASSERT(lhs.check_invariant());
	QUARK_ASSERT(rhs.check_invariant());

	if(type.is_string()){
		return concat_strings(backend, lhs, rhs);
	}
	else if(is_vector_carray(backend.types, backend.config, type)){
		return concat_vector_carray(backend, type, lhs, rhs);
	}
	else if(is_vector_hamt(backend.types, backend.config, type)){
		return concat_vector_hamt(backend, type, lhs, rhs);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}


uint64_t get_vector_size(value_backend_t& backend, const type_t& vector_type, runtime_value_t vec){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vector_type.check_invariant());
	QUARK_ASSERT(vec.check_invariant());

	if(vector_type.is_string()){
		return vec.vector_carray_ptr->get_element_count();
	}
	else if(is_vector_carray(backend.types, backend.config, vector_type)){
		return vec.vector_carray_ptr->get_element_count();
	}
	else if(is_vector_hamt(backend.types, backend.config, vector_type)){
		return vec.vector_hamt_ptr->get_element_count();
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}

runtime_value_t lookup_vector_element(value_backend_t& backend, const type_t& vector_type, runtime_value_t vec, uint64_t index){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(vector_type.check_invariant());
	QUARK_ASSERT(vec.check_invariant());
	QUARK_ASSERT(index >= 0);

	if(vector_type.is_string()){
		//??? slow
		const auto s = from_runtime_string2(backend, vec);
		return runtime_value_t { .int_value = s[index] };
	}
	else if(is_vector_carray(backend.types, backend.config, vector_type)){
#if DEBUG
		const auto size = vec.vector_carray_ptr->get_element_count();
		QUARK_ASSERT(index < size);
#endif

		return vec.vector_carray_ptr->get_element_ptr()[index];
	}
	else if(is_vector_hamt(backend.types, backend.config, vector_type)){
#if DEBUG
		const auto size = vec.vector_hamt_ptr->get_element_count();
		QUARK_ASSERT(index < size);
#endif

		return vec.vector_hamt_ptr->load_element(index);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}







runtime_value_t lookup_dict_cppmap(value_backend_t& backend, runtime_value_t dict, const type_t& dict_type, runtime_value_t key){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict_type.check_invariant());
	QUARK_ASSERT(is_dict_cppmap(backend.types, backend.config, dict_type));

	const auto& m = dict.dict_cppmap_ptr->get_map();
	const auto key_string = from_runtime_string2(backend, key);
	const auto it = m.find(key_string);
	if(it == m.end()){
		throw std::exception();
	}
	else{
		return it->second;
	}
}

//??? make faster key without creating std::string.
runtime_value_t lookup_dict_hamt(value_backend_t& backend, runtime_value_t dict, const type_t& dict_type, runtime_value_t key){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict_type.check_invariant());
	QUARK_ASSERT(is_dict_hamt(backend.types, backend.config, dict_type));

	const auto& m = dict.dict_hamt_ptr->get_map();
	const auto key_string = from_runtime_string2(backend, key);
	const auto it = m.find(key_string);
	if(it == nullptr){
		throw std::exception();
	}
	else{
		return *it;
	}
}

runtime_value_t lookup_dict(value_backend_t& backend, runtime_value_t dict, const type_t& dict_type, runtime_value_t key){
	if(is_dict_hamt(backend.types, backend.config, dict_type)){
		return lookup_dict_hamt(backend, dict, dict_type, key);
	}
	else if(is_dict_cppmap(backend.types, backend.config, dict_type)){
		return lookup_dict_cppmap(backend, dict, dict_type, key);
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}



uint64_t get_dict_size(value_backend_t& backend, const type_t& dict_type, runtime_value_t dict){
	QUARK_ASSERT(backend.check_invariant());
	QUARK_ASSERT(dict_type.check_invariant());
	QUARK_ASSERT(dict_type.is_dict());
	QUARK_ASSERT(dict.check_invariant());

	if(is_dict_hamt(backend.types, backend.config, dict_type)){
		return dict.dict_hamt_ptr->size();
	}
	else if(is_dict_cppmap(backend.types, backend.config, dict_type)){
		return dict.dict_cppmap_ptr->size();
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}




// Could specialize further, for vector_hamt<string>, vector_hamt<vector<x>> etc. But it's probably better to inline push_back() instead.


//??? Expensive to push_back since all elements in vector needs their RC bumped!
// Could specialize further, for vector_hamt<string>, vector_hamt<vector<x>> etc. But it's probably better to inline push_back() instead.

runtime_value_t push_back__string(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	QUARK_ASSERT(vec_type.is_string());

	auto value = from_runtime_string2(backend, vec);
	value.push_back((char)element.int_value);
	const auto result2 = to_runtime_string2(backend, value);
	return result2;
}

//??? use memcpy()
runtime_value_t push_back_carray_pod(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	const auto element_count = vec.vector_carray_ptr->get_element_count();
	auto source_ptr = vec.vector_carray_ptr->get_element_ptr();

	auto v2 = alloc_vector_carray(backend.heap, element_count + 1, element_count + 1, type_t(vec_type));
	auto dest_ptr = v2.vector_carray_ptr->get_element_ptr();
	for(int i = 0 ; i < element_count ; i++){
		dest_ptr[i] = source_ptr[i];
	}
	dest_ptr[element_count] = element;
	return v2;
}

runtime_value_t push_back_carray_nonpod(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	type_t element_itype = lookup_vector_element_type(backend, type_t(vec_type));
	const auto element_count = vec.vector_carray_ptr->get_element_count();
	auto source_ptr = vec.vector_carray_ptr->get_element_ptr();

	auto v2 = alloc_vector_carray(backend.heap, element_count + 1, element_count + 1, type_t(vec_type));
	auto dest_ptr = v2.vector_carray_ptr->get_element_ptr();
	for(int i = 0 ; i < element_count ; i++){
		dest_ptr[i] = source_ptr[i];
		retain_value(backend, dest_ptr[i], element_itype);
	}
	dest_ptr[element_count] = element;
	retain_value(backend, dest_ptr[element_count], element_itype);

	return v2;
}

runtime_value_t push_back_hamt_pod(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	return push_back_immutable_hamt(vec, element);
}

runtime_value_t push_back_hamt_nonpod(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	runtime_value_t vec2 = push_back_immutable_hamt(vec, element);
	type_t element_itype = lookup_vector_element_type(backend, type_t(vec_type));

	for(int i = 0 ; i < vec2.vector_hamt_ptr->get_element_count() ; i++){
		const auto& value = vec2.vector_hamt_ptr->load_element(i);
		retain_value(backend, value, element_itype);
	}
	return vec2;
}

runtime_value_t push_back2(value_backend_t& backend, runtime_value_t vec, const type_t& vec_type, runtime_value_t element){
	if(vec_type.is_string()){
		return push_back__string(backend, vec, vec_type, element);
	}
	else if(is_vector_carray(backend.types, backend.config, vec_type)){
		const auto is_rc = is_rc_value(backend.types, vec_type.get_vector_element_type(backend.types));

		if(is_rc){
			return push_back_carray_nonpod(backend, vec, vec_type, element);
		}
		else{
			return push_back_carray_pod(backend, vec, vec_type, element);
		}
	}
	else if(is_vector_hamt(backend.types, backend.config, vec_type)){
		const auto is_rc = is_rc_value(backend.types, vec_type.get_vector_element_type(backend.types));

		if(is_rc){
			return push_back_hamt_nonpod(backend, vec, vec_type, element);
		}
		else{
			return push_back_hamt_pod(backend, vec, vec_type, element);
		}
	}
	else{
		QUARK_ASSERT(false);
		throw std::exception();
	}
}









}	//	floyd

