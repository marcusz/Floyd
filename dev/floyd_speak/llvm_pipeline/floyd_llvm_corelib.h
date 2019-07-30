//
//  floyd_llvm_runtime.hpp
//  floyd_speak
//
//  Created by Marcus Zetterquist on 2019-04-24.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_llvm_corelib_hpp
#define floyd_llvm_corelib_hpp

#include <map>

namespace floyd {

std::map<std::string, void*> get_corelib_c_function_ptrs();

}	//	namespace floyd


#endif /* floyd_llvm_corelib_hpp */