/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

namespace oceanbase {
namespace logproxy {
enum EventType {

  UNKNOWN_EVENT = 0,
  START_EVENT_V3 = 1,
  QUERY_EVENT = 2,
  STOP_EVENT = 3,
  ROTATE_EVENT = 4,
  INTVAR_EVENT = 5,
  LOAD_EVENT = 6,
  SLAVE_EVENT = 7,
  CREATE_FILE_EVENT = 8,
  APPEND_BLOCK_EVENT = 9,
  EXEC_LOAD_EVENT = 10,
  DELETE_FILE_EVENT = 11,
  NEW_LOAD_EVENT = 12,
  RAND_EVENT = 13,
  USER_VAR_EVENT = 14,
  FORMAT_DESCRIPTION_EVENT = 15,
  XID_EVENT = 16,
  BEGIN_LOAD_QUERY_EVENT = 17,
  EXECUTE_LOAD_QUERY_EVENT = 18,
  TABLE_MAP_EVENT = 19,
  PRE_GA_WRITE_ROWS_EVENT = 20,
  PRE_GA_UPDATE_ROWS_EVENT = 21,
  PRE_GA_DELETE_ROWS_EVENT = 22,
  INCIDENT_EVENT = 26,
  HEARTBEAT_LOG_EVENT = 27,
  IGNORABLE_LOG_EVENT = 28,
  ROWS_QUERY_LOG_EVENT = 29,
  WRITE_ROWS_EVENT = 30,
  UPDATE_ROWS_EVENT = 31,
  DELETE_ROWS_EVENT = 32,
  GTID_LOG_EVENT = 33,
  ANONYMOUS_GTID_LOG_EVENT = 34,
  PREVIOUS_GTIDS_LOG_EVENT = 35,
  TRANSACTION_CONTEXT_EVENT = 36,
  VIEW_CHANGE_EVENT = 37,
  XA_PREPARE_LOG_EVENT = 38,
  ENUM_END_EVENT

};

inline static std::string event_type_to_str(EventType type)
{
  switch (type) {
    case QUERY_EVENT:
      return "Query";
    case ROTATE_EVENT:
      return "Rotate";
    case XID_EVENT:
      return "Xid";
    case USER_VAR_EVENT:
      return "User var";
    case FORMAT_DESCRIPTION_EVENT:
      return "Format_desc";
    case TABLE_MAP_EVENT:
      return "Table_map";
    case ROWS_QUERY_LOG_EVENT:
      return "Rows_query";
    case WRITE_ROWS_EVENT:
      return "Write_rows";
    case UPDATE_ROWS_EVENT:
      return "Update_rows";
    case DELETE_ROWS_EVENT:
      return "Delete_rows";
    case GTID_LOG_EVENT:
      return "Gtid";
    case ANONYMOUS_GTID_LOG_EVENT:
      return "Anonymous_Gtid";
    case PREVIOUS_GTIDS_LOG_EVENT:
      return "Previous_gtids";
    case HEARTBEAT_LOG_EVENT:
      return "Heartbeat";
    default:
      return "Unknown";
  }
}

}  // namespace logproxy
}  // namespace oceanbase
