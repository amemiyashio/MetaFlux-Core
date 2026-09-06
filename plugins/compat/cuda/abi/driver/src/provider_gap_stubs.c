/* GENERATED: typed stubs for driver symbols that cudart's initialization
   sweep requires but the managed backend does not implement yet. One thunk
   per symbol keeps runtime identification exact. Each call returns
   CUDA_ERROR_NOT_SUPPORTED; symbols graduate to functional implementations
   in provider.c through work-item-0.2.0.1. Regenerate the list from a
   METAFLUX_TRACE_GETPROCADDRESS run when the surface changes. */

#include "metaflux/cuda/abi.h"
#include <stddef.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>

static CUresult mf_gap_hit(const char* name) {
  if (getenv("METAFLUX_TRACE_STUBS") != NULL) {
    fprintf(stderr, "MF_GAP_CALL %s\n", name);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}



static CUresult mf_gap_cuArray3DCreate(void) { return mf_gap_hit("cuArray3DCreate"); }

static CUresult mf_gap_cuArray3DGetDescriptor(void) { return mf_gap_hit("cuArray3DGetDescriptor"); }

static CUresult mf_gap_cuArrayCreate(void) { return mf_gap_hit("cuArrayCreate"); }

static CUresult mf_gap_cuArrayDestroy(void) { return mf_gap_hit("cuArrayDestroy"); }

static CUresult mf_gap_cuArrayGetDescriptor(void) { return mf_gap_hit("cuArrayGetDescriptor"); }

static CUresult mf_gap_cuArrayGetMemoryRequirements(void) { return mf_gap_hit("cuArrayGetMemoryRequirements"); }

static CUresult mf_gap_cuArrayGetPlane(void) { return mf_gap_hit("cuArrayGetPlane"); }

static CUresult mf_gap_cuArrayGetSparseProperties(void) { return mf_gap_hit("cuArrayGetSparseProperties"); }

static CUresult mf_gap_cuDestroyExternalMemory(void) { return mf_gap_hit("cuDestroyExternalMemory"); }

static CUresult mf_gap_cuDestroyExternalSemaphore(void) { return mf_gap_hit("cuDestroyExternalSemaphore"); }

static CUresult mf_gap_cuDeviceGetGraphMemAttribute(void) { return mf_gap_hit("cuDeviceGetGraphMemAttribute"); }

static CUresult mf_gap_cuDeviceGetNvSciSyncAttributes(void) { return mf_gap_hit("cuDeviceGetNvSciSyncAttributes"); }

static CUresult mf_gap_cuDeviceGraphMemTrim(void) { return mf_gap_hit("cuDeviceGraphMemTrim"); }

static CUresult mf_gap_cuDevicePrimaryCtxGetState(void) { return mf_gap_hit("cuDevicePrimaryCtxGetState"); }

static CUresult mf_gap_cuDevicePrimaryCtxSetFlags(void) { return mf_gap_hit("cuDevicePrimaryCtxSetFlags"); }

static CUresult mf_gap_cuDeviceSetGraphMemAttribute(void) { return mf_gap_hit("cuDeviceSetGraphMemAttribute"); }

static CUresult mf_gap_cuEGLStreamConsumerAcquireFrame(void) { return mf_gap_hit("cuEGLStreamConsumerAcquireFrame"); }

static CUresult mf_gap_cuEGLStreamConsumerConnect(void) { return mf_gap_hit("cuEGLStreamConsumerConnect"); }

static CUresult mf_gap_cuEGLStreamConsumerConnectWithFlags(void) { return mf_gap_hit("cuEGLStreamConsumerConnectWithFlags"); }

static CUresult mf_gap_cuEGLStreamConsumerDisconnect(void) { return mf_gap_hit("cuEGLStreamConsumerDisconnect"); }

static CUresult mf_gap_cuEGLStreamConsumerReleaseFrame(void) { return mf_gap_hit("cuEGLStreamConsumerReleaseFrame"); }

static CUresult mf_gap_cuEGLStreamProducerConnect(void) { return mf_gap_hit("cuEGLStreamProducerConnect"); }

static CUresult mf_gap_cuEGLStreamProducerDisconnect(void) { return mf_gap_hit("cuEGLStreamProducerDisconnect"); }

static CUresult mf_gap_cuEGLStreamProducerPresentFrame(void) { return mf_gap_hit("cuEGLStreamProducerPresentFrame"); }

static CUresult mf_gap_cuEGLStreamProducerReturnFrame(void) { return mf_gap_hit("cuEGLStreamProducerReturnFrame"); }

static CUresult mf_gap_cuEventRecordWithFlags(void) { return mf_gap_hit("cuEventRecordWithFlags"); }

static CUresult mf_gap_cuExternalMemoryGetMappedBuffer(void) { return mf_gap_hit("cuExternalMemoryGetMappedBuffer"); }

static CUresult mf_gap_cuExternalMemoryGetMappedMipmappedArray(void) { return mf_gap_hit("cuExternalMemoryGetMappedMipmappedArray"); }

static CUresult mf_gap_cuFuncGetName(void) { return mf_gap_hit("cuFuncGetName"); }

static CUresult mf_gap_cuFuncGetParamInfo(void) { return mf_gap_hit("cuFuncGetParamInfo"); }

static CUresult mf_gap_cuGLCtxCreate(void) { return mf_gap_hit("cuGLCtxCreate"); }

static CUresult mf_gap_cuGLGetDevices(void) { return mf_gap_hit("cuGLGetDevices"); }

static CUresult mf_gap_cuGLInit(void) { return mf_gap_hit("cuGLInit"); }

static CUresult mf_gap_cuGLMapBufferObject(void) { return mf_gap_hit("cuGLMapBufferObject"); }

static CUresult mf_gap_cuGLMapBufferObjectAsync(void) { return mf_gap_hit("cuGLMapBufferObjectAsync"); }

static CUresult mf_gap_cuGLRegisterBufferObject(void) { return mf_gap_hit("cuGLRegisterBufferObject"); }

static CUresult mf_gap_cuGLSetBufferObjectMapFlags(void) { return mf_gap_hit("cuGLSetBufferObjectMapFlags"); }

static CUresult mf_gap_cuGLUnmapBufferObject(void) { return mf_gap_hit("cuGLUnmapBufferObject"); }

static CUresult mf_gap_cuGLUnmapBufferObjectAsync(void) { return mf_gap_hit("cuGLUnmapBufferObjectAsync"); }

static CUresult mf_gap_cuGLUnregisterBufferObject(void) { return mf_gap_hit("cuGLUnregisterBufferObject"); }

static CUresult mf_gap_cuGraphAddChildGraphNode(void) { return mf_gap_hit("cuGraphAddChildGraphNode"); }

static CUresult mf_gap_cuGraphAddDependencies(void) { return mf_gap_hit("cuGraphAddDependencies"); }

static CUresult mf_gap_cuGraphAddEmptyNode(void) { return mf_gap_hit("cuGraphAddEmptyNode"); }

static CUresult mf_gap_cuGraphAddEventRecordNode(void) { return mf_gap_hit("cuGraphAddEventRecordNode"); }

static CUresult mf_gap_cuGraphAddEventWaitNode(void) { return mf_gap_hit("cuGraphAddEventWaitNode"); }

static CUresult mf_gap_cuGraphAddExternalSemaphoresSignalNode(void) { return mf_gap_hit("cuGraphAddExternalSemaphoresSignalNode"); }

static CUresult mf_gap_cuGraphAddExternalSemaphoresWaitNode(void) { return mf_gap_hit("cuGraphAddExternalSemaphoresWaitNode"); }

static CUresult mf_gap_cuGraphAddHostNode(void) { return mf_gap_hit("cuGraphAddHostNode"); }

static CUresult mf_gap_cuGraphAddKernelNode(void) { return mf_gap_hit("cuGraphAddKernelNode"); }

static CUresult mf_gap_cuGraphAddMemAllocNode(void) { return mf_gap_hit("cuGraphAddMemAllocNode"); }

static CUresult mf_gap_cuGraphAddMemFreeNode(void) { return mf_gap_hit("cuGraphAddMemFreeNode"); }

static CUresult mf_gap_cuGraphAddMemcpyNode(void) { return mf_gap_hit("cuGraphAddMemcpyNode"); }

static CUresult mf_gap_cuGraphAddMemsetNode(void) { return mf_gap_hit("cuGraphAddMemsetNode"); }

static CUresult mf_gap_cuGraphChildGraphNodeGetGraph(void) { return mf_gap_hit("cuGraphChildGraphNodeGetGraph"); }

static CUresult mf_gap_cuGraphClone(void) { return mf_gap_hit("cuGraphClone"); }

static CUresult mf_gap_cuGraphCreate(void) { return mf_gap_hit("cuGraphCreate"); }

static CUresult mf_gap_cuGraphDestroy(void) { return mf_gap_hit("cuGraphDestroy"); }

static CUresult mf_gap_cuGraphDestroyNode(void) { return mf_gap_hit("cuGraphDestroyNode"); }

static CUresult mf_gap_cuGraphEventRecordNodeSetEvent(void) { return mf_gap_hit("cuGraphEventRecordNodeSetEvent"); }

static CUresult mf_gap_cuGraphEventWaitNodeSetEvent(void) { return mf_gap_hit("cuGraphEventWaitNodeSetEvent"); }

static CUresult mf_gap_cuGraphExecDestroy(void) { return mf_gap_hit("cuGraphExecDestroy"); }

static CUresult mf_gap_cuGraphExternalSemaphoresSignalNodeGetParams(void) { return mf_gap_hit("cuGraphExternalSemaphoresSignalNodeGetParams"); }

static CUresult mf_gap_cuGraphExternalSemaphoresSignalNodeSetParams(void) { return mf_gap_hit("cuGraphExternalSemaphoresSignalNodeSetParams"); }

static CUresult mf_gap_cuGraphExternalSemaphoresWaitNodeGetParams(void) { return mf_gap_hit("cuGraphExternalSemaphoresWaitNodeGetParams"); }

static CUresult mf_gap_cuGraphExternalSemaphoresWaitNodeSetParams(void) { return mf_gap_hit("cuGraphExternalSemaphoresWaitNodeSetParams"); }

static CUresult mf_gap_cuGraphGetEdges(void) { return mf_gap_hit("cuGraphGetEdges"); }

static CUresult mf_gap_cuGraphGetNodes(void) { return mf_gap_hit("cuGraphGetNodes"); }

static CUresult mf_gap_cuGraphGetRootNodes(void) { return mf_gap_hit("cuGraphGetRootNodes"); }

static CUresult mf_gap_cuGraphHostNodeGetParams(void) { return mf_gap_hit("cuGraphHostNodeGetParams"); }

static CUresult mf_gap_cuGraphHostNodeSetParams(void) { return mf_gap_hit("cuGraphHostNodeSetParams"); }

static CUresult mf_gap_cuGraphInstantiate(void) { return mf_gap_hit("cuGraphInstantiate"); }

static CUresult mf_gap_cuGraphInstantiateWithFlags(void) { return mf_gap_hit("cuGraphInstantiateWithFlags"); }

static CUresult mf_gap_cuGraphKernelNodeGetParams(void) { return mf_gap_hit("cuGraphKernelNodeGetParams"); }

static CUresult mf_gap_cuGraphKernelNodeSetParams(void) { return mf_gap_hit("cuGraphKernelNodeSetParams"); }

static CUresult mf_gap_cuGraphLaunch(void) { return mf_gap_hit("cuGraphLaunch"); }

static CUresult mf_gap_cuGraphMemAllocNodeGetParams(void) { return mf_gap_hit("cuGraphMemAllocNodeGetParams"); }

static CUresult mf_gap_cuGraphMemFreeNodeGetParams(void) { return mf_gap_hit("cuGraphMemFreeNodeGetParams"); }

static CUresult mf_gap_cuGraphMemcpyNodeGetParams(void) { return mf_gap_hit("cuGraphMemcpyNodeGetParams"); }

static CUresult mf_gap_cuGraphMemcpyNodeSetParams(void) { return mf_gap_hit("cuGraphMemcpyNodeSetParams"); }

static CUresult mf_gap_cuGraphMemsetNodeGetParams(void) { return mf_gap_hit("cuGraphMemsetNodeGetParams"); }

static CUresult mf_gap_cuGraphMemsetNodeSetParams(void) { return mf_gap_hit("cuGraphMemsetNodeSetParams"); }

static CUresult mf_gap_cuGraphNodeFindInClone(void) { return mf_gap_hit("cuGraphNodeFindInClone"); }

static CUresult mf_gap_cuGraphNodeGetDependencies(void) { return mf_gap_hit("cuGraphNodeGetDependencies"); }

static CUresult mf_gap_cuGraphNodeGetDependentNodes(void) { return mf_gap_hit("cuGraphNodeGetDependentNodes"); }

static CUresult mf_gap_cuGraphNodeGetType(void) { return mf_gap_hit("cuGraphNodeGetType"); }

static CUresult mf_gap_cuGraphRemoveDependencies(void) { return mf_gap_hit("cuGraphRemoveDependencies"); }

static CUresult mf_gap_cuGraphicsEGLRegisterImage(void) { return mf_gap_hit("cuGraphicsEGLRegisterImage"); }

static CUresult mf_gap_cuGraphicsGLRegisterBuffer(void) { return mf_gap_hit("cuGraphicsGLRegisterBuffer"); }

static CUresult mf_gap_cuGraphicsGLRegisterImage(void) { return mf_gap_hit("cuGraphicsGLRegisterImage"); }

static CUresult mf_gap_cuGraphicsMapResources(void) { return mf_gap_hit("cuGraphicsMapResources"); }

static CUresult mf_gap_cuGraphicsResourceGetMappedEglFrame(void) { return mf_gap_hit("cuGraphicsResourceGetMappedEglFrame"); }

static CUresult mf_gap_cuGraphicsResourceGetMappedMipmappedArray(void) { return mf_gap_hit("cuGraphicsResourceGetMappedMipmappedArray"); }

static CUresult mf_gap_cuGraphicsResourceGetMappedPointer(void) { return mf_gap_hit("cuGraphicsResourceGetMappedPointer"); }

static CUresult mf_gap_cuGraphicsResourceSetMapFlags(void) { return mf_gap_hit("cuGraphicsResourceSetMapFlags"); }

static CUresult mf_gap_cuGraphicsSubResourceGetMappedArray(void) { return mf_gap_hit("cuGraphicsSubResourceGetMappedArray"); }

static CUresult mf_gap_cuGraphicsUnmapResources(void) { return mf_gap_hit("cuGraphicsUnmapResources"); }

static CUresult mf_gap_cuGraphicsUnregisterResource(void) { return mf_gap_hit("cuGraphicsUnregisterResource"); }

static CUresult mf_gap_cuGraphicsVDPAURegisterOutputSurface(void) { return mf_gap_hit("cuGraphicsVDPAURegisterOutputSurface"); }

static CUresult mf_gap_cuGraphicsVDPAURegisterVideoSurface(void) { return mf_gap_hit("cuGraphicsVDPAURegisterVideoSurface"); }

static CUresult mf_gap_cuImportExternalMemory(void) { return mf_gap_hit("cuImportExternalMemory"); }

static CUresult mf_gap_cuImportExternalSemaphore(void) { return mf_gap_hit("cuImportExternalSemaphore"); }

static CUresult mf_gap_cuIpcCloseMemHandle(void) { return mf_gap_hit("cuIpcCloseMemHandle"); }

static CUresult mf_gap_cuIpcGetEventHandle(void) { return mf_gap_hit("cuIpcGetEventHandle"); }

static CUresult mf_gap_cuIpcGetMemHandle(void) { return mf_gap_hit("cuIpcGetMemHandle"); }

static CUresult mf_gap_cuIpcOpenEventHandle(void) { return mf_gap_hit("cuIpcOpenEventHandle"); }

static CUresult mf_gap_cuIpcOpenMemHandle(void) { return mf_gap_hit("cuIpcOpenMemHandle"); }

static CUresult mf_gap_cuLaunchCooperativeKernel(void) { return mf_gap_hit("cuLaunchCooperativeKernel"); }

static CUresult mf_gap_cuLaunchCooperativeKernelMultiDevice(void) { return mf_gap_hit("cuLaunchCooperativeKernelMultiDevice"); }

static CUresult mf_gap_cuLaunchHostFunc(void) { return mf_gap_hit("cuLaunchHostFunc"); }

static CUresult mf_gap_cuLaunchKernelEx(void) { return mf_gap_hit("cuLaunchKernelEx"); }

static CUresult mf_gap_cuLinkAddData(void) { return mf_gap_hit("cuLinkAddData"); }

static CUresult mf_gap_cuLinkAddFile(void) { return mf_gap_hit("cuLinkAddFile"); }

static CUresult mf_gap_cuLinkComplete(void) { return mf_gap_hit("cuLinkComplete"); }

static CUresult mf_gap_cuLinkCreate(void) { return mf_gap_hit("cuLinkCreate"); }

static CUresult mf_gap_cuLinkDestroy(void) { return mf_gap_hit("cuLinkDestroy"); }

static CUresult mf_gap_cuMemAdvise(void) { return mf_gap_hit("cuMemAdvise"); }

static CUresult mf_gap_cuMemAllocAsync(void) { return mf_gap_hit("cuMemAllocAsync"); }

static CUresult mf_gap_cuMemAllocFromPoolAsync(void) { return mf_gap_hit("cuMemAllocFromPoolAsync"); }

static CUresult mf_gap_cuMemFreeAsync(void) { return mf_gap_hit("cuMemFreeAsync"); }

static CUresult mf_gap_cuMemPoolCreate(void) { return mf_gap_hit("cuMemPoolCreate"); }

static CUresult mf_gap_cuMemPoolDestroy(void) { return mf_gap_hit("cuMemPoolDestroy"); }

static CUresult mf_gap_cuMemPoolExportPointer(void) { return mf_gap_hit("cuMemPoolExportPointer"); }

static CUresult mf_gap_cuMemPoolExportToShareableHandle(void) { return mf_gap_hit("cuMemPoolExportToShareableHandle"); }

static CUresult mf_gap_cuMemPoolGetAccess(void) { return mf_gap_hit("cuMemPoolGetAccess"); }

static CUresult mf_gap_cuMemPoolGetAttribute(void) { return mf_gap_hit("cuMemPoolGetAttribute"); }

static CUresult mf_gap_cuMemPoolImportFromShareableHandle(void) { return mf_gap_hit("cuMemPoolImportFromShareableHandle"); }

static CUresult mf_gap_cuMemPoolImportPointer(void) { return mf_gap_hit("cuMemPoolImportPointer"); }

static CUresult mf_gap_cuMemPoolSetAccess(void) { return mf_gap_hit("cuMemPoolSetAccess"); }

static CUresult mf_gap_cuMemPoolSetAttribute(void) { return mf_gap_hit("cuMemPoolSetAttribute"); }

static CUresult mf_gap_cuMemPoolTrimTo(void) { return mf_gap_hit("cuMemPoolTrimTo"); }

static CUresult mf_gap_cuMemPrefetchAsync(void) { return mf_gap_hit("cuMemPrefetchAsync"); }

static CUresult mf_gap_cuMemRangeGetAttribute(void) { return mf_gap_hit("cuMemRangeGetAttribute"); }

static CUresult mf_gap_cuMemRangeGetAttributes(void) { return mf_gap_hit("cuMemRangeGetAttributes"); }

static CUresult mf_gap_cuMemcpy(void) { return mf_gap_hit("cuMemcpy"); }

static CUresult mf_gap_cuMemcpy2DUnaligned(void) { return mf_gap_hit("cuMemcpy2DUnaligned"); }

static CUresult mf_gap_cuMemcpy3DPeer(void) { return mf_gap_hit("cuMemcpy3DPeer"); }

static CUresult mf_gap_cuMemcpy3DPeerAsync(void) { return mf_gap_hit("cuMemcpy3DPeerAsync"); }

static CUresult mf_gap_cuMemcpyAsync(void) { return mf_gap_hit("cuMemcpyAsync"); }

static CUresult mf_gap_cuMipmappedArrayCreate(void) { return mf_gap_hit("cuMipmappedArrayCreate"); }

static CUresult mf_gap_cuMipmappedArrayDestroy(void) { return mf_gap_hit("cuMipmappedArrayDestroy"); }

static CUresult mf_gap_cuMipmappedArrayGetLevel(void) { return mf_gap_hit("cuMipmappedArrayGetLevel"); }

static CUresult mf_gap_cuMipmappedArrayGetMemoryRequirements(void) { return mf_gap_hit("cuMipmappedArrayGetMemoryRequirements"); }

static CUresult mf_gap_cuMipmappedArrayGetSparseProperties(void) { return mf_gap_hit("cuMipmappedArrayGetSparseProperties"); }

static CUresult mf_gap_cuModuleGetGlobal(void) { return mf_gap_hit("cuModuleGetGlobal"); }

static CUresult mf_gap_cuModuleGetSurfRef(void) { return mf_gap_hit("cuModuleGetSurfRef"); }

static CUresult mf_gap_cuModuleGetTexRef(void) { return mf_gap_hit("cuModuleGetTexRef"); }

static CUresult mf_gap_cuModuleLoad(void) { return mf_gap_hit("cuModuleLoad"); }

static CUresult mf_gap_cuModuleLoadFatBinary(void) { return mf_gap_hit("cuModuleLoadFatBinary"); }

static CUresult mf_gap_cuOccupancyAvailableDynamicSMemPerBlock(void) { return mf_gap_hit("cuOccupancyAvailableDynamicSMemPerBlock"); }

static CUresult mf_gap_cuOccupancyMaxActiveClusters(void) { return mf_gap_hit("cuOccupancyMaxActiveClusters"); }

static CUresult mf_gap_cuOccupancyMaxPotentialClusterSize(void) { return mf_gap_hit("cuOccupancyMaxPotentialClusterSize"); }

static CUresult mf_gap_cuSignalExternalSemaphoresAsync(void) { return mf_gap_hit("cuSignalExternalSemaphoresAsync"); }

static CUresult mf_gap_cuStreamBeginCapture(void) { return mf_gap_hit("cuStreamBeginCapture"); }

static CUresult mf_gap_cuStreamBeginCaptureToGraph(void) { return mf_gap_hit("cuStreamBeginCaptureToGraph"); }

static CUresult mf_gap_cuStreamCreateWithPriority(void) { return mf_gap_hit("cuStreamCreateWithPriority"); }

static CUresult mf_gap_cuStreamEndCapture(void) { return mf_gap_hit("cuStreamEndCapture"); }

static CUresult mf_gap_cuStreamGetPriority(void) { return mf_gap_hit("cuStreamGetPriority"); }

static CUresult mf_gap_cuSurfObjectCreate(void) { return mf_gap_hit("cuSurfObjectCreate"); }

static CUresult mf_gap_cuSurfObjectDestroy(void) { return mf_gap_hit("cuSurfObjectDestroy"); }

static CUresult mf_gap_cuSurfObjectGetResourceDesc(void) { return mf_gap_hit("cuSurfObjectGetResourceDesc"); }

static CUresult mf_gap_cuTexObjectCreate(void) { return mf_gap_hit("cuTexObjectCreate"); }

static CUresult mf_gap_cuTexObjectDestroy(void) { return mf_gap_hit("cuTexObjectDestroy"); }

static CUresult mf_gap_cuTexObjectGetResourceDesc(void) { return mf_gap_hit("cuTexObjectGetResourceDesc"); }

static CUresult mf_gap_cuTexObjectGetResourceViewDesc(void) { return mf_gap_hit("cuTexObjectGetResourceViewDesc"); }

static CUresult mf_gap_cuTexObjectGetTextureDesc(void) { return mf_gap_hit("cuTexObjectGetTextureDesc"); }

static CUresult mf_gap_cuVDPAUCtxCreate(void) { return mf_gap_hit("cuVDPAUCtxCreate"); }

static CUresult mf_gap_cuVDPAUGetDevice(void) { return mf_gap_hit("cuVDPAUGetDevice"); }

static CUresult mf_gap_cuWaitExternalSemaphoresAsync(void) { return mf_gap_hit("cuWaitExternalSemaphoresAsync"); }


typedef CUresult (*mf_gap_fn)(void);

static const struct {
  const char* name;
  mf_gap_fn fn;
} mf_cuda_gap_table[] = {

  { "cuArray3DCreate", mf_gap_cuArray3DCreate },

  { "cuArray3DGetDescriptor", mf_gap_cuArray3DGetDescriptor },

  { "cuArrayCreate", mf_gap_cuArrayCreate },

  { "cuArrayDestroy", mf_gap_cuArrayDestroy },

  { "cuArrayGetDescriptor", mf_gap_cuArrayGetDescriptor },

  { "cuArrayGetMemoryRequirements", mf_gap_cuArrayGetMemoryRequirements },

  { "cuArrayGetPlane", mf_gap_cuArrayGetPlane },

  { "cuArrayGetSparseProperties", mf_gap_cuArrayGetSparseProperties },

  { "cuDestroyExternalMemory", mf_gap_cuDestroyExternalMemory },

  { "cuDestroyExternalSemaphore", mf_gap_cuDestroyExternalSemaphore },

  { "cuDeviceGetGraphMemAttribute", mf_gap_cuDeviceGetGraphMemAttribute },

  { "cuDeviceGetNvSciSyncAttributes", mf_gap_cuDeviceGetNvSciSyncAttributes },

  { "cuDeviceGraphMemTrim", mf_gap_cuDeviceGraphMemTrim },

  { "cuDevicePrimaryCtxGetState", mf_gap_cuDevicePrimaryCtxGetState },

  { "cuDevicePrimaryCtxSetFlags", mf_gap_cuDevicePrimaryCtxSetFlags },

  { "cuDeviceSetGraphMemAttribute", mf_gap_cuDeviceSetGraphMemAttribute },

  { "cuEGLStreamConsumerAcquireFrame", mf_gap_cuEGLStreamConsumerAcquireFrame },

  { "cuEGLStreamConsumerConnect", mf_gap_cuEGLStreamConsumerConnect },

  { "cuEGLStreamConsumerConnectWithFlags", mf_gap_cuEGLStreamConsumerConnectWithFlags },

  { "cuEGLStreamConsumerDisconnect", mf_gap_cuEGLStreamConsumerDisconnect },

  { "cuEGLStreamConsumerReleaseFrame", mf_gap_cuEGLStreamConsumerReleaseFrame },

  { "cuEGLStreamProducerConnect", mf_gap_cuEGLStreamProducerConnect },

  { "cuEGLStreamProducerDisconnect", mf_gap_cuEGLStreamProducerDisconnect },

  { "cuEGLStreamProducerPresentFrame", mf_gap_cuEGLStreamProducerPresentFrame },

  { "cuEGLStreamProducerReturnFrame", mf_gap_cuEGLStreamProducerReturnFrame },

  { "cuEventRecordWithFlags", mf_gap_cuEventRecordWithFlags },

  { "cuExternalMemoryGetMappedBuffer", mf_gap_cuExternalMemoryGetMappedBuffer },

  { "cuExternalMemoryGetMappedMipmappedArray", mf_gap_cuExternalMemoryGetMappedMipmappedArray },

  { "cuFuncGetName", mf_gap_cuFuncGetName },

  { "cuFuncGetParamInfo", mf_gap_cuFuncGetParamInfo },

  { "cuGLCtxCreate", mf_gap_cuGLCtxCreate },

  { "cuGLGetDevices", mf_gap_cuGLGetDevices },

  { "cuGLInit", mf_gap_cuGLInit },

  { "cuGLMapBufferObject", mf_gap_cuGLMapBufferObject },

  { "cuGLMapBufferObjectAsync", mf_gap_cuGLMapBufferObjectAsync },

  { "cuGLRegisterBufferObject", mf_gap_cuGLRegisterBufferObject },

  { "cuGLSetBufferObjectMapFlags", mf_gap_cuGLSetBufferObjectMapFlags },

  { "cuGLUnmapBufferObject", mf_gap_cuGLUnmapBufferObject },

  { "cuGLUnmapBufferObjectAsync", mf_gap_cuGLUnmapBufferObjectAsync },

  { "cuGLUnregisterBufferObject", mf_gap_cuGLUnregisterBufferObject },

  { "cuGraphAddChildGraphNode", mf_gap_cuGraphAddChildGraphNode },

  { "cuGraphAddDependencies", mf_gap_cuGraphAddDependencies },

  { "cuGraphAddEmptyNode", mf_gap_cuGraphAddEmptyNode },

  { "cuGraphAddEventRecordNode", mf_gap_cuGraphAddEventRecordNode },

  { "cuGraphAddEventWaitNode", mf_gap_cuGraphAddEventWaitNode },

  { "cuGraphAddExternalSemaphoresSignalNode", mf_gap_cuGraphAddExternalSemaphoresSignalNode },

  { "cuGraphAddExternalSemaphoresWaitNode", mf_gap_cuGraphAddExternalSemaphoresWaitNode },

  { "cuGraphAddHostNode", mf_gap_cuGraphAddHostNode },

  { "cuGraphAddKernelNode", mf_gap_cuGraphAddKernelNode },

  { "cuGraphAddMemAllocNode", mf_gap_cuGraphAddMemAllocNode },

  { "cuGraphAddMemFreeNode", mf_gap_cuGraphAddMemFreeNode },

  { "cuGraphAddMemcpyNode", mf_gap_cuGraphAddMemcpyNode },

  { "cuGraphAddMemsetNode", mf_gap_cuGraphAddMemsetNode },

  { "cuGraphChildGraphNodeGetGraph", mf_gap_cuGraphChildGraphNodeGetGraph },

  { "cuGraphClone", mf_gap_cuGraphClone },

  { "cuGraphCreate", mf_gap_cuGraphCreate },

  { "cuGraphDestroy", mf_gap_cuGraphDestroy },

  { "cuGraphDestroyNode", mf_gap_cuGraphDestroyNode },

  { "cuGraphEventRecordNodeSetEvent", mf_gap_cuGraphEventRecordNodeSetEvent },

  { "cuGraphEventWaitNodeSetEvent", mf_gap_cuGraphEventWaitNodeSetEvent },

  { "cuGraphExecDestroy", mf_gap_cuGraphExecDestroy },

  { "cuGraphExternalSemaphoresSignalNodeGetParams", mf_gap_cuGraphExternalSemaphoresSignalNodeGetParams },

  { "cuGraphExternalSemaphoresSignalNodeSetParams", mf_gap_cuGraphExternalSemaphoresSignalNodeSetParams },

  { "cuGraphExternalSemaphoresWaitNodeGetParams", mf_gap_cuGraphExternalSemaphoresWaitNodeGetParams },

  { "cuGraphExternalSemaphoresWaitNodeSetParams", mf_gap_cuGraphExternalSemaphoresWaitNodeSetParams },

  { "cuGraphGetEdges", mf_gap_cuGraphGetEdges },

  { "cuGraphGetNodes", mf_gap_cuGraphGetNodes },

  { "cuGraphGetRootNodes", mf_gap_cuGraphGetRootNodes },

  { "cuGraphHostNodeGetParams", mf_gap_cuGraphHostNodeGetParams },

  { "cuGraphHostNodeSetParams", mf_gap_cuGraphHostNodeSetParams },

  { "cuGraphInstantiate", mf_gap_cuGraphInstantiate },

  { "cuGraphInstantiateWithFlags", mf_gap_cuGraphInstantiateWithFlags },

  { "cuGraphKernelNodeGetParams", mf_gap_cuGraphKernelNodeGetParams },

  { "cuGraphKernelNodeSetParams", mf_gap_cuGraphKernelNodeSetParams },

  { "cuGraphLaunch", mf_gap_cuGraphLaunch },

  { "cuGraphMemAllocNodeGetParams", mf_gap_cuGraphMemAllocNodeGetParams },

  { "cuGraphMemFreeNodeGetParams", mf_gap_cuGraphMemFreeNodeGetParams },

  { "cuGraphMemcpyNodeGetParams", mf_gap_cuGraphMemcpyNodeGetParams },

  { "cuGraphMemcpyNodeSetParams", mf_gap_cuGraphMemcpyNodeSetParams },

  { "cuGraphMemsetNodeGetParams", mf_gap_cuGraphMemsetNodeGetParams },

  { "cuGraphMemsetNodeSetParams", mf_gap_cuGraphMemsetNodeSetParams },

  { "cuGraphNodeFindInClone", mf_gap_cuGraphNodeFindInClone },

  { "cuGraphNodeGetDependencies", mf_gap_cuGraphNodeGetDependencies },

  { "cuGraphNodeGetDependentNodes", mf_gap_cuGraphNodeGetDependentNodes },

  { "cuGraphNodeGetType", mf_gap_cuGraphNodeGetType },

  { "cuGraphRemoveDependencies", mf_gap_cuGraphRemoveDependencies },

  { "cuGraphicsEGLRegisterImage", mf_gap_cuGraphicsEGLRegisterImage },

  { "cuGraphicsGLRegisterBuffer", mf_gap_cuGraphicsGLRegisterBuffer },

  { "cuGraphicsGLRegisterImage", mf_gap_cuGraphicsGLRegisterImage },

  { "cuGraphicsMapResources", mf_gap_cuGraphicsMapResources },

  { "cuGraphicsResourceGetMappedEglFrame", mf_gap_cuGraphicsResourceGetMappedEglFrame },

  { "cuGraphicsResourceGetMappedMipmappedArray", mf_gap_cuGraphicsResourceGetMappedMipmappedArray },

  { "cuGraphicsResourceGetMappedPointer", mf_gap_cuGraphicsResourceGetMappedPointer },

  { "cuGraphicsResourceSetMapFlags", mf_gap_cuGraphicsResourceSetMapFlags },

  { "cuGraphicsSubResourceGetMappedArray", mf_gap_cuGraphicsSubResourceGetMappedArray },

  { "cuGraphicsUnmapResources", mf_gap_cuGraphicsUnmapResources },

  { "cuGraphicsUnregisterResource", mf_gap_cuGraphicsUnregisterResource },

  { "cuGraphicsVDPAURegisterOutputSurface", mf_gap_cuGraphicsVDPAURegisterOutputSurface },

  { "cuGraphicsVDPAURegisterVideoSurface", mf_gap_cuGraphicsVDPAURegisterVideoSurface },

  { "cuImportExternalMemory", mf_gap_cuImportExternalMemory },

  { "cuImportExternalSemaphore", mf_gap_cuImportExternalSemaphore },

  { "cuIpcCloseMemHandle", mf_gap_cuIpcCloseMemHandle },

  { "cuIpcGetEventHandle", mf_gap_cuIpcGetEventHandle },

  { "cuIpcGetMemHandle", mf_gap_cuIpcGetMemHandle },

  { "cuIpcOpenEventHandle", mf_gap_cuIpcOpenEventHandle },

  { "cuIpcOpenMemHandle", mf_gap_cuIpcOpenMemHandle },

  { "cuLaunchCooperativeKernel", mf_gap_cuLaunchCooperativeKernel },

  { "cuLaunchCooperativeKernelMultiDevice", mf_gap_cuLaunchCooperativeKernelMultiDevice },

  { "cuLaunchHostFunc", mf_gap_cuLaunchHostFunc },

  { "cuLaunchKernelEx", mf_gap_cuLaunchKernelEx },

  { "cuLinkAddData", mf_gap_cuLinkAddData },

  { "cuLinkAddFile", mf_gap_cuLinkAddFile },

  { "cuLinkComplete", mf_gap_cuLinkComplete },

  { "cuLinkCreate", mf_gap_cuLinkCreate },

  { "cuLinkDestroy", mf_gap_cuLinkDestroy },

  { "cuMemAdvise", mf_gap_cuMemAdvise },

  { "cuMemAllocAsync", mf_gap_cuMemAllocAsync },

  { "cuMemAllocFromPoolAsync", mf_gap_cuMemAllocFromPoolAsync },

  { "cuMemFreeAsync", mf_gap_cuMemFreeAsync },

  { "cuMemPoolCreate", mf_gap_cuMemPoolCreate },

  { "cuMemPoolDestroy", mf_gap_cuMemPoolDestroy },

  { "cuMemPoolExportPointer", mf_gap_cuMemPoolExportPointer },

  { "cuMemPoolExportToShareableHandle", mf_gap_cuMemPoolExportToShareableHandle },

  { "cuMemPoolGetAccess", mf_gap_cuMemPoolGetAccess },

  { "cuMemPoolGetAttribute", mf_gap_cuMemPoolGetAttribute },

  { "cuMemPoolImportFromShareableHandle", mf_gap_cuMemPoolImportFromShareableHandle },

  { "cuMemPoolImportPointer", mf_gap_cuMemPoolImportPointer },

  { "cuMemPoolSetAccess", mf_gap_cuMemPoolSetAccess },

  { "cuMemPoolSetAttribute", mf_gap_cuMemPoolSetAttribute },

  { "cuMemPoolTrimTo", mf_gap_cuMemPoolTrimTo },

  { "cuMemPrefetchAsync", mf_gap_cuMemPrefetchAsync },

  { "cuMemRangeGetAttribute", mf_gap_cuMemRangeGetAttribute },

  { "cuMemRangeGetAttributes", mf_gap_cuMemRangeGetAttributes },

  { "cuMemcpy", mf_gap_cuMemcpy },

  { "cuMemcpy2DUnaligned", mf_gap_cuMemcpy2DUnaligned },

  { "cuMemcpy3DPeer", mf_gap_cuMemcpy3DPeer },

  { "cuMemcpy3DPeerAsync", mf_gap_cuMemcpy3DPeerAsync },

  { "cuMemcpyAsync", mf_gap_cuMemcpyAsync },

  { "cuMipmappedArrayCreate", mf_gap_cuMipmappedArrayCreate },

  { "cuMipmappedArrayDestroy", mf_gap_cuMipmappedArrayDestroy },

  { "cuMipmappedArrayGetLevel", mf_gap_cuMipmappedArrayGetLevel },

  { "cuMipmappedArrayGetMemoryRequirements", mf_gap_cuMipmappedArrayGetMemoryRequirements },

  { "cuMipmappedArrayGetSparseProperties", mf_gap_cuMipmappedArrayGetSparseProperties },

  { "cuModuleGetGlobal", mf_gap_cuModuleGetGlobal },

  { "cuModuleGetSurfRef", mf_gap_cuModuleGetSurfRef },

  { "cuModuleGetTexRef", mf_gap_cuModuleGetTexRef },

  { "cuModuleLoad", mf_gap_cuModuleLoad },

  { "cuModuleLoadFatBinary", mf_gap_cuModuleLoadFatBinary },

  { "cuOccupancyAvailableDynamicSMemPerBlock", mf_gap_cuOccupancyAvailableDynamicSMemPerBlock },

  { "cuOccupancyMaxActiveClusters", mf_gap_cuOccupancyMaxActiveClusters },

  { "cuOccupancyMaxPotentialClusterSize", mf_gap_cuOccupancyMaxPotentialClusterSize },

  { "cuSignalExternalSemaphoresAsync", mf_gap_cuSignalExternalSemaphoresAsync },

  { "cuStreamBeginCapture", mf_gap_cuStreamBeginCapture },

  { "cuStreamBeginCaptureToGraph", mf_gap_cuStreamBeginCaptureToGraph },

  { "cuStreamCreateWithPriority", mf_gap_cuStreamCreateWithPriority },

  { "cuStreamEndCapture", mf_gap_cuStreamEndCapture },

  { "cuStreamGetPriority", mf_gap_cuStreamGetPriority },

  { "cuSurfObjectCreate", mf_gap_cuSurfObjectCreate },

  { "cuSurfObjectDestroy", mf_gap_cuSurfObjectDestroy },

  { "cuSurfObjectGetResourceDesc", mf_gap_cuSurfObjectGetResourceDesc },

  { "cuTexObjectCreate", mf_gap_cuTexObjectCreate },

  { "cuTexObjectDestroy", mf_gap_cuTexObjectDestroy },

  { "cuTexObjectGetResourceDesc", mf_gap_cuTexObjectGetResourceDesc },

  { "cuTexObjectGetResourceViewDesc", mf_gap_cuTexObjectGetResourceViewDesc },

  { "cuTexObjectGetTextureDesc", mf_gap_cuTexObjectGetTextureDesc },

  { "cuVDPAUCtxCreate", mf_gap_cuVDPAUCtxCreate },

  { "cuVDPAUGetDevice", mf_gap_cuVDPAUGetDevice },

  { "cuWaitExternalSemaphoresAsync", mf_gap_cuWaitExternalSemaphoresAsync },

};

void* mf_cuda_gap_lookup(const char* symbol) {
  for (size_t i = 0; i < sizeof(mf_cuda_gap_table) / sizeof(mf_cuda_gap_table[0]); ++i) {
    if (strcmp(symbol, mf_cuda_gap_table[i].name) == 0) {
      return (void*)mf_cuda_gap_table[i].fn;
    }
  }
  return (void*)0;
}
