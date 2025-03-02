#ifndef GrVkGraphicCoreTraceInterface_DEFINED
#define GrVkGraphicCoreTraceInterface_DEFINED

#include "include/gpu/vk/GrVkGraphicCoreTrace.h"

#if defined(SK_VULKAN) && defined(SKIA_DFX_FOR_GPURESOURCE_CORETRACE)
#define RECORD_GPURESOURCE_CORETRACE_CALLER(FunctionType) \
    GraphicCoreTrace::RecordCoreTrace(FunctionType)
#else
#define RECORD_GPURESOURCE_CORETRACE_CALLER(FunctionType)
#endif

#if defined(SK_VULKAN) && defined(SKIA_DFX_FOR_GPURESOURCE_CORETRACE)
#define RECORD_GPURESOURCE_CORETRACE_CALLER_WITHNODEID(FunctionType, NodeId) \
    GraphicCoreTrace::RecordCoreTrace(FunctionType, NodeId)
#else
#define RECORD_GPURESOURCE_CORETRACE_CALLER_WITHNODEID(FunctionType, NodeId)
#endif

#endif