 /*
 * Copyright (C) 2014 the contributors as stated in the AUTHORS file
 *
 * This file is part of open62541. open62541 is free software: you can
 * redistribute it and/or modify it under the terms of the GNU Lesser General
 * Public License, version 3 (as published by the Free Software Foundation) with
 * a static linking exception as stated in the LICENSE file provided with
 * open62541.
 *
 * open62541 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#ifndef UA_SERVER_H_
#define UA_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_types.h"
#include "ua_util.h"
#include "ua_types_generated.h"
#include "ua_connection.h"
#include "ua_log.h"

/**
 * @defgroup server Server
 *
 * @{
 */

struct UA_Server;
typedef struct UA_Server UA_Server;

UA_Server UA_EXPORT * UA_Server_new(void);
void UA_EXPORT UA_Server_setServerCertificate(UA_Server *server, UA_ByteString certificate);
void UA_EXPORT UA_Server_delete(UA_Server *server);

/**
 * Runs the main loop of the server. In each iteration, this calls into the
 * networklayers to see if work have arrived and checks if timed events need to
 * be triggered.
 *
 * @param server The server object
 * @param nThreads The number of worker threads. Is ignored if MULTITHREADING is
 * not activated.
 * @param running Points to a booloean value on the heap. When running is set to
 * false, the worker threads and the main loop close and the server is shut
 * down.
 * @return Indicates whether the server shut down cleanly
 *
 */
UA_StatusCode UA_EXPORT UA_Server_run(UA_Server *server, UA_UInt16 nThreads, UA_Boolean *running);

/**
 * Add a node to the server's address space
 *
 * If adding the node succeeds, the pointer to the node is set to null. If the
 * original nodeid is null (ns=0,i=0), a unique new nodeid is created for the
 * node and returned in the AddNodesResult struct. */
UA_AddNodesResult UA_EXPORT UA_Server_addNode(UA_Server *server, const UA_Node **node,
                                              const UA_ExpandedNodeId *parentNodeId,
                                              const UA_NodeId *referenceTypeId);

/** Add a reference to the server's address space */
UA_StatusCode UA_EXPORT UA_Server_addReference(UA_Server *server, const UA_AddReferencesItem *item);

/**
 * Add a scalar variable (node) to the server's address space
 *
 * The value must lie on the heap and must not be reused after adding it, as it
 * becomes attached to the lifecycle of the VariableNode. */
void UA_EXPORT UA_Server_addScalarVariableNode(UA_Server *server, UA_QualifiedName *browseName,
                                               void *value, const UA_TypeVTable *vt,
                                               const UA_ExpandedNodeId *parentNodeId,
                                               const UA_NodeId *referenceTypeId );

/** Work that is run in the main loop (singlethreaded) or dispatched to a worker
    thread. */
typedef struct UA_WorkItem {
    enum {
        UA_WORKITEMTYPE_NOTHING,
        UA_WORKITEMTYPE_BINARYNETWORKMESSAGE,
        UA_WORKITEMTYPE_METHODCALL,
        UA_WORKITEMTYPE_DELAYEDMETHODCALL,
    } type;
    union {
        struct {
            UA_Connection *connection;
            UA_ByteString message;
        } binaryNetworkMessage;
        struct {
            void * data;
            void (*method)(UA_Server *server, void *data);
        } methodCall;
    } work;
} UA_WorkItem;

/**
 * Add work that is executed at a given time in the future. If the indicated
 * time lies in the past, the work is executed immediately.
 *
 * The work pointer is not freed but copied to an internal representation
 */
UA_Guid UA_EXPORT UA_Server_addTimedWorkItem(UA_Server *server, UA_WorkItem *work, UA_DateTime time);

/**
 *  Add work that is executed repeatedly with the given interval (in 100ns). If
 *  work with the same repetition interval already exists, the first execution
 *  might occur sooner.
 *
 * The work pointer is not freed but copied to an internal representation
 */
UA_Guid UA_EXPORT UA_Server_addRepeatedWorkItem(UA_Server *server, UA_WorkItem *work, UA_UInt32 interval);

/** Remove timed or repeated work */
UA_Boolean UA_EXPORT UA_Server_removeWorkItem(UA_Server *server, UA_Guid workId);

/**
 * Interface to the binary network layers. This structure is returned from the
 * function that initializes the network layer. The layer is already bound to a
 * specific port and listening. The functions in the structure are never called
 * in parallel but only sequentially from the server's main loop. So the network
 * layer does not need to be thread-safe.
 */
typedef struct {
    void *nlHandle;

    /**
     * Starts listening on the the networklayer.
     *
     * @return Returns UA_STATUSCODE_GOOD or an error code.
     */
    UA_StatusCode (*start)(void *nlHandle);
    
    /**
     * Gets called from the main server loop and returns the work that
     * accumulated (messages and close events) for dispatch. The networklayer
     * does not wait on connections but returns immediately the work that
     * accumulated.
     *
     * @param workItems When the returned integer is positive, *workItems points
     * to an array of WorkItems of the returned size.
     * @param timeout The timeout during which an event must arrive.
     * @return The size of the returned workItems array. If the result is
     * negative, an error has occured.
     */
    UA_Int32 (*getWork)(void *nlhandle, UA_WorkItem **workItems, UA_UInt16 timeout);

    /**
     * Closes the network connection and returns all the work that needs to
     * be finished before the network layer can be safely deleted.
     *
     * @param workItems When the returned integer is positive, *workItems points
     * to an array of WorkItems of the returned size.
     * @return The size of the returned workItems array. If the result is
     * negative, an error has occured.
     */
    UA_Int32 (*stop)(void *nlhandle, UA_WorkItem **workItems);

    /** Deletes the network layer. Call only after a successfull shutdown. */
    void (*delete)(void *nlhandle);
} UA_NetworkLayer;

/**
 * Adds a network layer to the server. The network layer is destroyed together
 * with the server. Do not use it after adding it as it might be moved around on
 * the heap.
 */
void UA_EXPORT UA_Server_addNetworkLayer(UA_Server *server, UA_NetworkLayer networkLayer);

/** @} */

/**
 * @ingroup nodestore
 *
 * @defgroup external_nodestore External Nodestore
 *
 * @brief An external application that manages its own data and data model
 *
 * To plug in outside data sources, one can use
 *
 * - VariableNodes with a data source (functions that are called for read and write access)
 * - An external nodestore that is mapped to specific namespaces
 *
 * If no external nodestore is defined for a nodeid, it is always looked up in
 * the "local" nodestore of open62541. Namespace Zero is always in the local
 * nodestore.
 *
 * @{
 */

typedef UA_Int32 (*UA_ExternalNodeStore_addNodes)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_AddNodesItem *nodesToAdd, UA_UInt32 *indices,
 UA_UInt32 indicesSize, UA_AddNodesResult* addNodesResults, UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_addReferences)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_AddReferencesItem* referencesToAdd,
 UA_UInt32 *indices,UA_UInt32 indicesSize, UA_StatusCode *addReferencesResults,
 UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_deleteNodes)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_DeleteNodesItem *nodesToDelete, UA_UInt32 *indices,
 UA_UInt32 indicesSize, UA_StatusCode *deleteNodesResults, UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_deleteReferences)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_DeleteReferencesItem *referenceToDelete,
 UA_UInt32 *indices, UA_UInt32 indicesSize, UA_StatusCode deleteReferencesresults,
 UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_readNodes)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_ReadValueId *readValueIds, UA_UInt32 *indices,
 UA_UInt32 indicesSize,UA_DataValue *readNodesResults, UA_Boolean timeStampToReturn,
 UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_writeNodes)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_WriteValue *writeValues, UA_UInt32 *indices,
 UA_UInt32 indicesSize, UA_StatusCode *writeNodesResults, UA_DiagnosticInfo *diagnosticInfo);

typedef UA_Int32 (*UA_ExternalNodeStore_browseNodes)
(void *ensHandle, const UA_RequestHeader *requestHeader, UA_BrowseDescription *browseDescriptions,
 UA_UInt32 *indices, UA_UInt32 indicesSize, UA_UInt32 requestedMaxReferencesPerNode,
 UA_BrowseResult *browseResults, UA_DiagnosticInfo *diagnosticInfos);

typedef UA_Int32 (*UA_ExternalNodeStore_delete)(void *ensHandle);

typedef struct UA_ExternalNodeStore {
    void *ensHandle;
	UA_ExternalNodeStore_addNodes addNodes;
	UA_ExternalNodeStore_deleteNodes deleteNodes;
	UA_ExternalNodeStore_writeNodes writeNodes;
	UA_ExternalNodeStore_readNodes readNodes;
	UA_ExternalNodeStore_browseNodes browseNodes;
	UA_ExternalNodeStore_addReferences addReferences;
	UA_ExternalNodeStore_deleteReferences deleteReferences;
	UA_ExternalNodeStore_delete delete;
} UA_ExternalNodeStore;

UA_StatusCode UA_EXPORT
UA_Server_addExternalNamespace(UA_Server *server, UA_UInt16 namespaceIndex, UA_ExternalNodeStore *nodeStore);

/** @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_SERVER_H_ */
