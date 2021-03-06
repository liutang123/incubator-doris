// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef DORIS_BE_SRC_OLAP_TASK_ENGINE_SCHEMA_CHANGE_TASK_H
#define DORIS_BE_SRC_OLAP_TASK_ENGINE_SCHEMA_CHANGE_TASK_H

#include "gen_cpp/AgentService_types.h"
#include "olap/olap_define.h"
#include "olap/task/engine_task.h"

namespace doris {

// base class for storage engine
// add "Engine" as task prefix to prevent duplicate name with agent task
class EngineSchemaChangeTask : public EngineTask {

public:
    virtual OLAPStatus execute();

public:
    EngineSchemaChangeTask(const TAlterTabletReq& alter_tablet_request, int64_t signature,
        const TTaskType::type task_type, vector<string>* error_msgs, const string& process_name);
    ~EngineSchemaChangeTask() {}

private:
    // ######################### ALTER TABLE BEGIN #########################
    // The following interfaces are all about alter tablet operation, 
    // the main logical is that generating a new tablet with different
    // schema on base tablet.
    
    // Create rollup tablet on base tablet, after create_rollup_tablet,
    // both base tablet and new tablet is effective.
    //
    // @param [in] request specify base tablet, new tablet and its schema
    // @return OLAP_SUCCESS if submit success
    OLAPStatus _create_rollup_tablet(const TAlterTabletReq& request);

    // Do schema change on tablet, StorageEngine support
    // add column, drop column, alter column type and order,
    // after schema_change, base tablet is abandoned.
    // Note that the two tablets has same tablet_id but different schema_hash
    // 
    // @param [in] request specify base tablet, new tablet and its schema
    // @return OLAP_SUCCESS if submit success
    OLAPStatus _schema_change(const TAlterTabletReq& request);

private:
    const TAlterTabletReq& _alter_tablet_req;
    int64_t _signature;
    const TTaskType::type _task_type;
    vector<string>* _error_msgs;
    const string& _process_name;

}; // EngineTask

} // doris
#endif //DORIS_BE_SRC_OLAP_TASK_ENGINE_SCHEMA_CHANGE_TASK_H