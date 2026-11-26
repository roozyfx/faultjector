/* Explicit instantiation of templates to avoid LLVM code generation bugs */
#include <string>

#include "fault_pipe.h"
#include "perturb.h"

template class fxcore::Perturb<std::string>;
template class fxcore::FaultPipe<std::string>;
