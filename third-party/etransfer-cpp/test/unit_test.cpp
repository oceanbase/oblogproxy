/**
 * Copyright (c) 2024 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "common/define.h"
#include "convert/convert_tool.h"
#include "convert/ddl_parser_context.h"
#include "convert/sql_builder_context.h"
#include "gtest/gtest.h"
#include "object/object.h"
#include "sink/mysql_builder.h"
#include "source/obmysql_ddl_source.h"

using etransfer::source::ParseContext;
using etransfer::sink::BuildContext;
using etransfer::sink::MySqlBuilder;
using etransfer::source::OBMySQLDDLSource;

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(DROP_TABLE, DROP_TABLE_1) {
  std::string source = "drop table t;";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);
  EXPECT_STREQ("DROP TABLE `t`", dest.c_str());
}

// new case
TEST(DROP_TABLE, DROP_TABLE_2) {
  std::string source = "drop table t, tt;";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);
  EXPECT_STREQ("DROP TABLE `t`,`tt`", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_LIKE) {
  std::string source = "CREATE TABLE  IF NOT EXISTS T LIKE OTHERT";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);
  EXPECT_STREQ("CREATE TABLE  IF NOT EXISTS `t` LIKE `othert`", dest.c_str());
}

TEST(TRUNCATE_TABLE, TRUNCATE_TABLE_1) {
  std::string source = "truncate table t;";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);
  EXPECT_STREQ("TRUNCATE TABLE `t`", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CASE_SENSITIVE_1) {
  std::string source = "create table Tt (Cc1 int, `Cc2` int)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ("CREATE TABLE `tt`(\n\t`Cc1` INTEGER,\n\t`Cc2` INTEGER\n)", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CASE_SENSITIVE_2) {
  std::string source = "create table `Tt` (Cc1 int, `Cc2` int)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ("CREATE TABLE `tt`(\n\t`Cc1` INTEGER,\n\t`Cc2` INTEGER\n)", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CASE_SENSITIVE_3) {
  std::string source = "create table Tt (Cc1 int, `Cc2` int)";
  std::string dest;
  std::string err_msg;
  etransfer::tool::ConvertTool::Parse(source, "", true, dest, err_msg);

  EXPECT_STREQ("CREATE TABLE `tt`(\n\t`Cc1` INTEGER,\n\t`Cc2` INTEGER\n)", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CASE_SENSITIVE_4) {
  std::string source = "create table `Tt` (Cc1 int, `Cc2` int)";
  std::string dest;
  std::string err_msg;
  etransfer::tool::ConvertTool::Parse(source, "", true, dest, err_msg);

  EXPECT_STREQ("CREATE TABLE `Tt`(\n\t`Cc1` INTEGER,\n\t`Cc2` INTEGER\n)", dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_TYPE_1) {
  std::string source = "CREATE TABLE t1 (c1 JSON)";
  std::string expect = "CREATE TABLE `t1`(\n\t`c1` JSON\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_TYPE_2) {
  std::string source = "create table t(c1 fixed, c2 fixed(10), c3 fixed(10,2));";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`c1` DECIMAL,\n"
                        "\t`c2` DECIMAL (10),\n"
                        "\t`c3` DECIMAL (10, 2)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_TYPE_3) {
  std::string source = "CREATE TABLE T (cfloat1 float(7,0), cdouble1 double(7,0), cfloat2 float(7,2), "
                        "cdouble2 double(7,2), cfloat3 float(7), cdouble3 double, cfloat4 float, cfloat5 float(26))";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`cfloat1` FLOAT (7, 0),\n"
                        "\t`cdouble1` DOUBLE (7, 0),\n"
                        "\t`cfloat2` FLOAT (7, 2),\n"
                        "\t`cdouble2` DOUBLE (7, 2),\n"
                        "\t`cfloat3` FLOAT,\n"
                        "\t`cdouble3` DOUBLE,\n"
                        "\t`cfloat4` FLOAT,\n"
                        "\t`cfloat5` DOUBLE\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_TYPE_4) {
  std::string source = "CREATE TABLE T (cfloat1 float, cdouble1 double, cfloat2 float(7,2), "
                        "cdouble2 double(7,2))";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`cfloat1` FLOAT,\n"
                        "\t`cdouble1` DOUBLE,\n"
                        "\t`cfloat2` FLOAT (7, 2),\n"
                        "\t`cdouble2` DOUBLE (7, 2)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_ALL_TPYES) {
  std::string source = "CREATE TABLE `T`(\n"
                        "\t`UCTINYINT` TINYINT UNSIGNED,\n"
                        "\t`CTINYINT` TINYINT,\n"
                        "\t`UCSMALLINT` SMALLINT UNSIGNED,\n"
                        "\t`CSMALLINT` SMALLINT,\n"
                        "\t`UCMEDIUMINT` MEDIUMINT UNSIGNED,\n"
                        "\t`CMEDIUMINT` MEDIUMINT,\n"
                        "\t`UCINT` INTEGER UNSIGNED,\n"
                        "\t`CINT` INTEGER,\n"
                        "\t`UCINTEGER` INTEGER UNSIGNED,\n"
                        "\t`CINTEGER` INTEGER,\n"
                        "\t`UCBIGINT` BIGINT UNSIGNED,\n"
                        "\t`CBIGINT` BIGINT,\n"
                        "\t`CFLOAT` FLOAT (10, 2),\n"
                        "\t`CDOUBLE` DOUBLE,\n"
                        "\t`CDEC` DECIMAL (10, 0),\n"
                        "\t`CNUMERIC` NUMERIC,\n"
                        "\t`CDECIMAL` DECIMAL (13, 2),\n"
                        "\t`CBOOL` BOOLEAN,\n"
                        "\t`CDATETIME` DATETIME (6),\n"
                        "\t`CTIMESTAMP` TIMESTAMP (3) NOT NULL ,\n"
                        "\t`CTIME` TIME,\n"
                        "\t`CDATE` DATE,\n"
                        "\t`CYEAR` YEAR,\n"
                        "\t`CCHAR` CHAR (4),\n"
                        "\t`CCHARACTER` CHAR (8),\n"
                        "\t`CVARCHAR` VARCHAR (16),\n"
                        "\t`CTINYTEXT` TINYTEXT,\n"
                        "\t`CTEXT` TEXT,\n"
                        "\t`CMEDIUMTEXT` MEDIUMTEXT,\n"
                        "\t`CLONGTEXT` LONGTEXT,\n"
                        "\t`CTINYBLOB` TINYBLOB,\n"
                        "\t`CBLOB` BLOB,\n"
                        "\t`CMEDIUMBLOB` MEDIUMBLOB,\n"
                        "\t`CLONGBLOB` LONGBLOB,\n"
                        "\t`CBINARY` BINARY (32),\n"
                        "\t`CVARBINARY` VARBINARY (64),\n"
                        "\t`CBIT` BIT (64),\n"
                        "\t`CENUM` ENUM('a','b','bc'),\n"
                        "\t`CSET` SET('abc','b','abcde')\n"
                        ")\n";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`UCTINYINT` TINYINT UNSIGNED,\n"
                        "\t`CTINYINT` TINYINT,\n"
                        "\t`UCSMALLINT` SMALLINT UNSIGNED,\n"
                        "\t`CSMALLINT` SMALLINT,\n"
                        "\t`UCMEDIUMINT` MEDIUMINT UNSIGNED,\n"
                        "\t`CMEDIUMINT` MEDIUMINT,\n"
                        "\t`UCINT` INTEGER UNSIGNED,\n"
                        "\t`CINT` INTEGER,\n"
                        "\t`UCINTEGER` INTEGER UNSIGNED,\n"
                        "\t`CINTEGER` INTEGER,\n"
                        "\t`UCBIGINT` BIGINT UNSIGNED,\n"
                        "\t`CBIGINT` BIGINT,\n"
                        "\t`CFLOAT` FLOAT (10, 2),\n"
                        "\t`CDOUBLE` DOUBLE,\n"
                        "\t`CDEC` DECIMAL (10, 0),\n"
                        "\t`CNUMERIC` NUMERIC,\n"
                        "\t`CDECIMAL` DECIMAL (13, 2),\n"
                        "\t`CBOOL` BOOLEAN,\n"
                        "\t`CDATETIME` DATETIME (6),\n"
                        "\t`CTIMESTAMP` TIMESTAMP (3) NOT NULL ,\n"
                        "\t`CTIME` TIME,\n"
                        "\t`CDATE` DATE,\n"
                        "\t`CYEAR` YEAR,\n"
                        "\t`CCHAR` CHAR (4),\n"
                        "\t`CCHARACTER` CHAR (8),\n"
                        "\t`CVARCHAR` VARCHAR (16),\n"
                        "\t`CTINYTEXT` TINYTEXT,\n"
                        "\t`CTEXT` TEXT,\n"
                        "\t`CMEDIUMTEXT` MEDIUMTEXT,\n"
                        "\t`CLONGTEXT` LONGTEXT,\n"
                        "\t`CTINYBLOB` TINYBLOB,\n"
                        "\t`CBLOB` BLOB,\n"
                        "\t`CMEDIUMBLOB` MEDIUMBLOB,\n"
                        "\t`CLONGBLOB` LONGBLOB,\n"
                        "\t`CBINARY` BINARY (32),\n"
                        "\t`CVARBINARY` VARBINARY (64),\n"
                        "\t`CBIT` BIT (64),\n"
                        "\t`CENUM` ENUM('a','b','bc'),\n"
                        "\t`CSET` SET('abc','b','abcde')\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}


// new case
TEST(ALTER_TABLE, ALTER_TABLE) {
  std::string source = "alter table testiq8z7y add column c7 int;";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ("ALTER TABLE `testiq8z7y`\n\t\tADD COLUMN `c7` INTEGER\n", dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_1) {
  std::string source = "CREATE TABLE `sub_listkey` (\n"
        "  `userid` int(11),\n"
        "  `usertype` int(11)\n"
        ") DEFAULT CHARSET = utf8mb4 ROW_FORMAT = DYNAMIC COMPRESSION = 'zstd_1.0' REPLICA_NUM = 1 BLOCK_SIZE = 16384 USE_BLOOM_FILTER = FALSE TABLET_SIZE = 134217728 PCTFREE = 0\n";
  std::string expect = "CREATE TABLE `sub_listkey`(\n"
        "\t`userid` INTEGER (11),\n"
        "\t`usertype` INTEGER (11)\n"
        ") CHARACTER SET = utf8mb4";

  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_1) {
  std::string source = "create table t (\n"
                        "uctinyint   tinyint   unsigned not null primary key, \n"
                        "ucsmallint  smallint  unsigned unique key, \n"
                        "csmallint   smallint  not null);";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`uctinyint` TINYINT UNSIGNED NOT NULL ,\n"
                        "\t`ucsmallint` SMALLINT UNSIGNED,\n"
                        "\t`csmallint` SMALLINT NOT NULL ,\n"
                        "\tPRIMARY KEY (`uctinyint`),\n"
                        "\tUNIQUE KEY `GENE_1` (`ucsmallint`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_2) {
  std::string source = "CREATE TABLE T(\n"
                        "\tUCTINYINT TINYINT UNSIGNED NOT NULL ,\n"
                        "\tUCSMALLINT SMALLINT UNSIGNED,\n"
                        "\tCSMALLINT SMALLINT NOT NULL ,\n"
                        "\tUNIQUE KEY (UCSMALLINT) ,\n"
                        "\tPRIMARY KEY (UCTINYINT),\n"
                        "\tCONSTRAINT T_CHECK_0 CHECK (UCSMALLINT > 10 )\n"
                        ")";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`UCTINYINT` TINYINT UNSIGNED NOT NULL ,\n"
                        "\t`UCSMALLINT` SMALLINT UNSIGNED,\n"
                        "\t`CSMALLINT` SMALLINT NOT NULL ,\n"
                        "\tUNIQUE KEY (`UCSMALLINT`),\n"
                        "\tPRIMARY KEY (`UCTINYINT`),\n"
                        "\tCONSTRAINT `T_CHECK_0` CHECK (`UCSMALLINT` > 10 )\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_3) {
  std::string source = "create table t1 (c1 int check(c1 > 10));";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER\tCHECK (`c1` > 10 ) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_4) {
  std::string source = "create table t1 (c1 int , constraint a check(c1 > 10));";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER,\n"
                        "\tCONSTRAINT `a` CHECK (`c1` > 10 )\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_5) {
  std::string source = "create table t1 (c1 int , check(c1 > 10));";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER,\n"
                        "\tCHECK (`c1` > 10 )\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_6) {
  std::string source = "CREATE TABLE t2_ddl(CHECK (c1<>c2),c1 INT CHECK (c1>10),c2 INT CONSTRAINT c2_positive CHECK (c2>0),c3 INT CHECK(c3<100),CONSTRAINT c1_nonzero CHECK(c1<>0),CHECK(c1>c3))";
  std::string expect = "CREATE TABLE `t2_ddl`(\n"
                        "\t`c1` INTEGER\tCHECK (`c1` > 10 ) ,\n"
                        "\t`c2` INTEGER\tCONSTRAINT `c2_positive` CHECK (`c2` > 0 ) ,\n"
                        "\t`c3` INTEGER\tCHECK (`c3` < 100 ) ,\n"
                        "\tCHECK (`c1` <> `c2` ),\n"
                        "\tCONSTRAINT `c1_nonzero` CHECK (`c1` <> 0 ),\n"
                        "\tCHECK (`c1` > `c3` )\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_ZEROFILL) {
  std::string source = "CREATE TABLE `t`(\n"
                        "  `uctinyint` TINYINT UNSIGNED,\n"
                        "  `ctinyint` TINYINT ZEROFILL,\n"
                        "  `ucsmallint` SMALLINT UNSIGNED ZEROFILL,\n"
                        "  `csmallint` SMALLINT SIGNED\n"
                        ");";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`uctinyint` TINYINT UNSIGNED,\n"
                        "\t`ctinyint` TINYINT ZEROFILL,\n"
                        "\t`ucsmallint` SMALLINT UNSIGNED ZEROFILL,\n"
                        "\t`csmallint` SMALLINT\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}


TEST(CREATE_TABLE, CREATE_TABLE_WITH_KEY_1) {
  std::string source = "create table t1(c1 int not null primary key, c2 char(32));";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\t`c2` CHAR (32),\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_KEY_2) {
  std::string source = "CREATE TABLE `list_default_test`(\n"
                        "\t`id` INTEGER (11) NOT NULL ,\n"
                        "\t`name` VARCHAR (10) NOT NULL ,\n"
                        "\tPRIMARY KEY (`id`,`name`))\n";
  std::string expect = "CREATE TABLE `list_default_test`(\n"
                        "\t`id` INTEGER (11) NOT NULL ,\n"
                        "\t`name` VARCHAR (10) NOT NULL ,\n"
                        "\tPRIMARY KEY (`id`,`name`)\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_KEY_3) {
  std::string source = "CREATE TABLE `tms_failed_jobs` (\n"
                "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,\n"
                "  `mq_connection` varchar(50) NOT NULL,\n"
                "  PRIMARY KEY (`id`) USING BTREE,\n"
                "  KEY `idx_mq_connection` (`mq_connection`)\n"
                ") ENGINE=InnoDB AUTO_INCREMENT=1963 DEFAULT CHARSET=utf8mb4;";
  std::string expect = "CREATE TABLE `tms_failed_jobs`(\n"
                "\t`id` BIGINT (20) UNSIGNED NOT NULL  AUTO_INCREMENT ,\n"
                "\t`mq_connection` VARCHAR (50) NOT NULL ,\n"
                "\tPRIMARY KEY (`id`),\n"
                "\tINDEX `idx_mq_connection`(`mq_connection`) \n"
                ") CHARACTER SET = utf8mb4";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_1) {
  std::string source = "CREATE TABLE k1 (\n"
                        "    id INT NOT NULL,\n"
                        "    name VARCHAR(20),\n"
                        "    UNIQUE KEY (id)\n"
                        ")\n"
                        "PARTITION BY KEY()\n"
                        "PARTITIONS 2;";
  std::string expect = "CREATE TABLE `k1`(\n"
                        "\t`id` INTEGER NOT NULL ,\n"
                        "\t`name` VARCHAR (20),\n"
                        "\tUNIQUE KEY (`id`)\n"
                        ") PARTITION BY KEY () PARTITIONS 2";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_2) {
  std::string source = "create table test_range (id int,name varchar(20),date datetime,primary key(id,date)) "
                "partition by range columns(date)(partition p1 values less than ('2022-02-03 00:00:00'),"
                "partition p2 values less than ('2022-03-03 00:00:00'));";
  std::string expect = "CREATE TABLE `test_range`(\n\t`id` INTEGER NOT NULL ,\n\t`name` VARCHAR (20),\n"
                "\t`date` DATETIME NOT NULL ,\n\tPRIMARY KEY (`id`,`date`)\n"
                ") PARTITION BY RANGE COLUMNS (`date`) \n(\n"
                "\tPARTITION `p1` VALUES LESS THAN ('2022-02-03 00:00:00'),\n"
                "\tPARTITION `p2` VALUES LESS THAN ('2022-03-03 00:00:00')\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_3) {
  std::string source = "create table test_range (id int,name varchar(20),date datetime,primary key(id,date)) "
                "partition by range (id)(partition p1 values less than (1),"
                "partition p2 values less than (2));";
  std::string expect = "CREATE TABLE `test_range`(\n\t`id` INTEGER NOT NULL ,\n\t`name` VARCHAR (20),\n"
                "\t`date` DATETIME NOT NULL ,\n\tPRIMARY KEY (`id`,`date`)\n) PARTITION BY RANGE (`id`) \n"
                "(\n\tPARTITION `p1` VALUES LESS THAN (1),\n\tPARTITION `p2` VALUES LESS THAN (2)\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_4) {
  std::string source = "CREATE TABLE tm1 (\n"
                        " s1 CHAR(32) PRIMARY KEY\n"
                        ")\n"
                        "PARTITION BY KEY(s1)\n"
                        "PARTITIONS 10;";
  std::string expect = "CREATE TABLE `tm1`(\n"
                        "\t`s1` CHAR (32) NOT NULL ,\n"
                        "\tPRIMARY KEY (`s1`)\n"
                        ") PARTITION BY KEY (`s1`) PARTITIONS 10";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_5) {
  std::string source = "CREATE TABLE `list_default_test`(\n"
                        "\t`id` INTEGER (11) NOT NULL ,\n"
                        "\t`name` VARCHAR (10) NOT NULL ,\n"
                        "\tPRIMARY KEY (`id`,`name`)\n"
                        ") PARTITION BY LIST COLUMNS (`ID`) \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES IN(1, 3, 5, 7, 9),\n"
                        "\tPARTITION `P1` VALUES IN(2, 4, 6, 8),\n"
                        "\tPARTITION `P2` VALUES IN(10)\n"
                        ")";
  std::string expect = "CREATE TABLE `list_default_test`(\n"
                        "\t`id` INTEGER (11) NOT NULL ,\n"
                        "\t`name` VARCHAR (10) NOT NULL ,\n"
                        "\tPRIMARY KEY (`id`,`name`)\n"
                        ") PARTITION BY LIST COLUMNS (`ID`) \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES IN(1, 3, 5, 7, 9),\n"
                        "\tPARTITION `P1` VALUES IN(2, 4, 6, 8),\n"
                        "\tPARTITION `P2` VALUES IN(10)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_6) {
  std::string source = "CREATE TABLE `range_columns_test` (\n"
                        "  `id` INT(11) NOT NULL,\n"
                        "  `departno` BIGINT(20) NOT NULL,\n"
                        "  PRIMARY KEY (`id`, `departno`)\n"
                        ")\n"
                        " PARTITION BY RANGE COLUMNS(id,departno)\n"
                        "(PARTITION p0 VALUES LESS THAN (100,1000),\n"
                        "PARTITION p1 VALUES LESS THAN (100,2000),\n"
                        "PARTITION p2 VALUES LESS THAN (200,1000),\n"
                        "PARTITION p4 VALUES LESS THAN (200,2000),\n"
                        "PARTITION pmax VALUES LESS THAN (MAXVALUE,MAXVALUE));";
  std::string expect = "CREATE TABLE `range_columns_test`(\n"
                        "\t`id` INTEGER (11) NOT NULL ,\n"
                        "\t`departno` BIGINT (20) NOT NULL ,\n"
                        "\tPRIMARY KEY (`id`,`departno`)\n"
                        ") PARTITION BY RANGE COLUMNS (`id`,`departno`) \n"
                        "(\n"
                        "\tPARTITION `p0` VALUES LESS THAN (100,1000),\n"
                        "\tPARTITION `p1` VALUES LESS THAN (100,2000),\n"
                        "\tPARTITION `p2` VALUES LESS THAN (200,1000),\n"
                        "\tPARTITION `p4` VALUES LESS THAN (200,2000),\n"
                        "\tPARTITION `pmax` VALUES LESS THAN (MAXVALUE,MAXVALUE)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_7) {
  std::string source = "CREATE TABLE  IF NOT EXISTS `NON_PARTRANGE_KEY_PRI`(\n"
                        "\t`COL1` INTEGER,\n"
                        "\t`COL2` NUMERIC (10, 2),\n"
                        "\t`COL3` DECIMAL (10, 2),\n"
                        "\t`COL4` BIT,\n"
                        "\t`COL5` TINYINT,\n"
                        "\t`COL6` SMALLINT,\n"
                        "\t`COL7` MEDIUMINT,\n"
                        "\t`COL8` BIGINT,\n"
                        "\t`COL9` FLOAT (10, 2),\n"
                        "\t`COL10` DOUBLE (10, 2),\n"
                        "\t`COL11` VARCHAR (10),\n"
                        "\t`COL12` CHAR (10),\n"
                        "\t`COL13` TEXT,\n"
                        "\t`COL14` TINYTEXT,\n"
                        "\t`COL15` MEDIUMTEXT,\n"
                        "\t`COL16` LONGTEXT,\n"
                        "\t`COL17` BLOB,\n"
                        "\t`COL18` TINYBLOB,\n"
                        "\t`CLO19` LONGBLOB,\n"
                        "\t`COL20` MEDIUMBLOB,\n"
                        "\t`COL21` BINARY (16),\n"
                        "\t`COL22` VARBINARY (16),\n"
                        "\t`COL23` TIMESTAMP NULL ,\n"
                        "\t`COL24` TIME,\n"
                        "\t`COL25` DATE,\n"
                        "\t`COL26` DATETIME,\n"
                        "\t`COL27` YEAR,\n"
                        "\tPRIMARY KEY (`COL1`,`COL6`)\n"
                        ") PARTITION BY RANGE COLUMNS (`COL6`) SUBPARTITION BY KEY(`col1`) \n"
                        "(\n"
                        "\tPARTITION `P1` VALUES LESS THAN (0)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P1_1`,\n"
                        "\t\tSUBPARTITION `P1_2`,\n"
                        "\t\tSUBPARTITION `P1_3`\n"
                        "\t),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (10000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P2_1`,\n"
                        "\t\tSUBPARTITION `P2_2`,\n"
                        "\t\tSUBPARTITION `P2_3`\n"
                        "\t),\n"
                        "\tPARTITION `P3` VALUES LESS THAN (100000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P3_1`,\n"
                        "\t\tSUBPARTITION `P3_2`,\n"
                        "\t\tSUBPARTITION `P3_3`\n"
                        "\t),\n"
                        "\tPARTITION `P4` VALUES LESS THAN (maxvalue)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P4_1`,\n"
                        "\t\tSUBPARTITION `P4_2`,\n"
                        "\t\tSUBPARTITION `P4_3`\n"
                        "\t)\n"
                        ")";
  std::string expect = "CREATE TABLE  IF NOT EXISTS `non_partrange_key_pri`(\n"
                        "\t`COL1` INTEGER NOT NULL ,\n"
                        "\t`COL2` NUMERIC (10, 2),\n"
                        "\t`COL3` DECIMAL (10, 2),\n"
                        "\t`COL4` BIT,\n"
                        "\t`COL5` TINYINT,\n"
                        "\t`COL6` SMALLINT NOT NULL ,\n"
                        "\t`COL7` MEDIUMINT,\n"
                        "\t`COL8` BIGINT,\n"
                        "\t`COL9` FLOAT (10, 2),\n"
                        "\t`COL10` DOUBLE (10, 2),\n"
                        "\t`COL11` VARCHAR (10),\n"
                        "\t`COL12` CHAR (10),\n"
                        "\t`COL13` TEXT,\n"
                        "\t`COL14` TINYTEXT,\n"
                        "\t`COL15` MEDIUMTEXT,\n"
                        "\t`COL16` LONGTEXT,\n"
                        "\t`COL17` BLOB,\n"
                        "\t`COL18` TINYBLOB,\n"
                        "\t`CLO19` LONGBLOB,\n"
                        "\t`COL20` MEDIUMBLOB,\n"
                        "\t`COL21` BINARY (16),\n"
                        "\t`COL22` VARBINARY (16),\n"
                        "\t`COL23` TIMESTAMP NULL ,\n"
                        "\t`COL24` TIME,\n"
                        "\t`COL25` DATE,\n"
                        "\t`COL26` DATETIME,\n"
                        "\t`COL27` YEAR,\n"
                        "\tPRIMARY KEY (`COL1`,`COL6`)\n"
                        ") PARTITION BY RANGE COLUMNS (`COL6`) SUBPARTITION BY KEY(`col1`) \n"
                        "(\n"
                        "\tPARTITION `P1` VALUES LESS THAN (0)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P1_1`,\n"
                        "\t\tSUBPARTITION `P1_2`,\n"
                        "\t\tSUBPARTITION `P1_3`\n"
                        "\t),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (10000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P2_1`,\n"
                        "\t\tSUBPARTITION `P2_2`,\n"
                        "\t\tSUBPARTITION `P2_3`\n"
                        "\t),\n"
                        "\tPARTITION `P3` VALUES LESS THAN (100000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P3_1`,\n"
                        "\t\tSUBPARTITION `P3_2`,\n"
                        "\t\tSUBPARTITION `P3_3`\n"
                        "\t),\n"
                        "\tPARTITION `P4` VALUES LESS THAN (maxvalue)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `P4_1`,\n"
                        "\t\tSUBPARTITION `P4_2`,\n"
                        "\t\tSUBPARTITION `P4_3`\n"
                        "\t)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_8) {
  std::string source = "CREATE TABLE `TS`(\n"
                        "\t`ID` INTEGER,\n"
                        "\t`PURCHASED` DATE\n"
                        ") PARTITION BY RANGE (YEAR ( `PURCHASED` )) SUBPARTITION BY HASH(TO_DAYS ( `PURCHASED` )) SUBPARTITIONS 2 \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES LESS THAN (1990),\n"
                        "\tPARTITION `P1` VALUES LESS THAN (2000),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (MAXVALUE)\n"
                        ")";
  std::string expect = "CREATE TABLE `ts`(\n"
                        "\t`ID` INTEGER,\n"
                        "\t`PURCHASED` DATE\n"
                        ") PARTITION BY RANGE (YEAR ( `PURCHASED` )) SUBPARTITION BY HASH(TO_DAYS ( `PURCHASED` )) SUBPARTITIONS 2 \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES LESS THAN (1990),\n"
                        "\tPARTITION `P1` VALUES LESS THAN (2000),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (MAXVALUE)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_9) {
  std::string source = "CREATE TABLE `TS`(\n"
                        "\t`ID` INTEGER,\n"
                        "\t`PURCHASED` DATE\n"
                        ") PARTITION BY RANGE (YEAR ( `PURCHASED` )) SUBPARTITION BY HASH(TO_DAYS ( `PURCHASED` )) \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES LESS THAN (1990)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S0`,\n"
                        "\t\tSUBPARTITION `S1`\n"
                        "\t),\n"
                        "\tPARTITION `P1` VALUES LESS THAN (2000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S2`,\n"
                        "\t\tSUBPARTITION `S3`\n"
                        "\t),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (MAXVALUE)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S4`,\n"
                        "\t\tSUBPARTITION `S5`\n"
                        "\t)\n"
                        ")";
  std::string expect = "CREATE TABLE `ts`(\n"
                        "\t`ID` INTEGER,\n"
                        "\t`PURCHASED` DATE\n"
                        ") PARTITION BY RANGE (YEAR ( `PURCHASED` )) SUBPARTITION BY HASH(TO_DAYS ( `PURCHASED` )) \n"
                        "(\n"
                        "\tPARTITION `P0` VALUES LESS THAN (1990)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S0`,\n"
                        "\t\tSUBPARTITION `S1`\n"
                        "\t),\n"
                        "\tPARTITION `P1` VALUES LESS THAN (2000)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S2`,\n"
                        "\t\tSUBPARTITION `S3`\n"
                        "\t),\n"
                        "\tPARTITION `P2` VALUES LESS THAN (MAXVALUE)\n"
                        "\t(\n"
                        "\t\tSUBPARTITION `S4`,\n"
                        "\t\tSUBPARTITION `S5`\n"
                        "\t)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_10) {
  std::string source = "CREATE TABLE test_comment_partition(\n"
                        "c1 BIGINT PRIMARY KEY,\n"
                        "c2 VARCHAR(50)\n"
                        ")  COMMENT = '测试表' PARTITION BY LIST(c1)\n"
                        "(\n"
                        "PARTITION p0 VALUES IN (1, 2, 3),\n"
                        "PARTITION p1 VALUES IN (5, 6),\n"
                        "PARTITION p2 VALUES IN (7, 8)\n"
                        ");0";
  std::string expect = "CREATE TABLE `test_comment_partition`(\n"
                        "\t`c1` BIGINT NOT NULL ,\n"
                        "\t`c2` VARCHAR (50),\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")  COMMENT '测试表' PARTITION BY LIST (`c1`) \n"
                        "(\n"
                        "\tPARTITION `p0` VALUES IN(1, 2, 3),\n"
                        "\tPARTITION `p1` VALUES IN(5, 6),\n"
                        "\tPARTITION `p2` VALUES IN(7, 8)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_11) {
  std::string source = "create table t_hash_range001 (c1 int, c2 int, c3 int, c4 int)\n"
                        "partition by hash(c1)\n"
                        "subpartition by range(c2)\n"
                        "subpartition template\n"
                        "(\n"
                        "  subpartition rsp0 values less than (100),\n"
                        "  subpartition rsp1 values less than (200),\n"
                        "  subpartition rsp2 values less than (300)\n"
                        ")\n"
                        "partitions 3;";
  std::string expect = "CREATE TABLE `t_hash_range001`(\n"
                        "\t`c1` INTEGER,\n"
                        "\t`c2` INTEGER,\n"
                        "\t`c3` INTEGER,\n"
                        "\t`c4` INTEGER\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}


TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_12) {
  std::string source = "create table if not exists partrange_range_nopri(\n"
                        "        col1 int,col6 smallint) \n"
                        "        partition by range(col1) subpartition by range(col6) subpartition template\n"
                        "        (subpartition p_1 values less than (1),\n"
                        "        subpartition p_2 values less than (10),\n"
                        "        subpartition p_3 values less than (100),\n"
                        "        subpartition p_4 values less than maxvalue)\n"
                        "        (partition p1 values less than(0),\n"
                        "        partition p2 values less than(10000),\n"
                        "        partition p3 values less than(100000),\n"
                        "        partition p4 values less than maxvalue);";
  std::string expect = "CREATE TABLE  IF NOT EXISTS `partrange_range_nopri`(\n"
                        "\t`col1` INTEGER,\n"
                        "\t`col6` SMALLINT\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_13) {
  std::string source = "CREATE TABLE ts (id INT, purchased DATE)"
        "PARTITION BY RANGE( YEAR(purchased) )"
        "SUBPARTITION BY HASH( TO_DAYS(purchased) )"
        "SUBPARTITIONS 2 ("
        "PARTITION p0 VALUES LESS THAN (1990),"
        "PARTITION p1 VALUES LESS THAN (2000),"
        "PARTITION p2 VALUES LESS THAN MAXVALUE"
        ");";
  std::string expect = "CREATE TABLE `ts`(\n\t`id` INTEGER,\n\t`purchased` DATE\n)"
        " PARTITION BY RANGE (YEAR ( `purchased` )) SUBPARTITION BY HASH(TO_DAYS ( `purchased` ))"
        " SUBPARTITIONS 2 \n(\n\tPARTITION `p0` VALUES LESS THAN (1990),"
        "\n\tPARTITION `p1` VALUES LESS THAN (2000),"
        "\n\tPARTITION `p2` VALUES LESS THAN (MAXVALUE)\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_14) {
  std::string source = "CREATE TABLE t2_f_rr(col1 INT,col2 TIMESTAMP) "
       "PARTITION BY RANGE(col1)"
       "SUBPARTITION BY RANGE(UNIX_TIMESTAMP(col2))"
        "(PARTITION p0 VALUES LESS THAN(100)"
          " (SUBPARTITION sp0 VALUES LESS THAN(UNIX_TIMESTAMP('2021/04/01')),"
          "  SUBPARTITION sp1 VALUES LESS THAN(UNIX_TIMESTAMP('2021/07/01')),"
          "  SUBPARTITION sp2 VALUES LESS THAN(UNIX_TIMESTAMP('2021/10/01')),"
          "  SUBPARTITION sp3 VALUES LESS THAN(UNIX_TIMESTAMP('2022/01/01'))"
          " ),"
         "PARTITION p1 VALUES LESS THAN(200)"
          " (SUBPARTITION sp4 VALUES LESS THAN(UNIX_TIMESTAMP('2021/04/01')),"
          "  SUBPARTITION sp5 VALUES LESS THAN(UNIX_TIMESTAMP('2021/07/01')),"
          "  SUBPARTITION sp6 VALUES LESS THAN(UNIX_TIMESTAMP('2021/10/01')),"
          "  SUBPARTITION sp7 VALUES LESS THAN(UNIX_TIMESTAMP('2022/01/01'))"
          "  )"
         ");";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_15) {
  std::string source = "CREATE TABLE t2_f_rlc (col1 INT NOT NULL,col2 varchar(50),col3 INT NOT NULL) \n" 
                        "PARTITION BY RANGE(col1)\n" 
                        "SUBPARTITION BY LIST COLUMNS(col3)\n" 
                        "(PARTITION p0 VALUES LESS THAN(100)\n" 
                        "  (SUBPARTITION sp0 VALUES IN(1,3),\n" 
                        "   SUBPARTITION sp1 VALUES IN(4,6),\n" 
                        "   SUBPARTITION sp2 VALUES IN(7,9)),\n" 
                        " PARTITION p1 VALUES LESS THAN(200)\n" 
                        "  (SUBPARTITION sp3 VALUES IN(1,3),\n" 
                        "   SUBPARTITION sp4 VALUES IN(4,6),\n" 
                        "   SUBPARTITION sp5 VALUES IN(7,9))\n" 
                        "); ";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_16) {
  std::string source = "CREATE TABLE t2_f_rck (col1 INT NOT NULL,col2 varchar(50),col3 INT NOT NULL) \n" 
                        "PARTITION BY RANGE COLUMNS(col1)\n" 
                        "SUBPARTITION BY KEY(col3)\n" 
                        "(PARTITION p0 VALUES LESS THAN(100)\n" 
                        "  (SUBPARTITION sp0,\n" 
                        "   SUBPARTITION sp1,\n" 
                        "   SUBPARTITION sp2),\n" 
                        " PARTITION p1 VALUES LESS THAN(200)\n" 
                        "  (SUBPARTITION sp3,\n" 
                        "   SUBPARTITION sp4,\n" 
                        "   SUBPARTITION sp5)\n" 
                        "); " ;
  std::string expect = "CREATE TABLE `t2_f_rck`("
                        "\n\t`col1` INTEGER NOT NULL ,"
                        "\n\t`col2` VARCHAR (50),"
                        "\n\t`col3` INTEGER NOT NULL \n)"
                        " PARTITION BY RANGE COLUMNS (`col1`)"
                        " SUBPARTITION BY KEY(`col3`) \n("
                        "\n\tPARTITION `p0` VALUES LESS THAN (100)\n\t("
                        "\n\t\tSUBPARTITION `sp0`,\n\t\tSUBPARTITION `sp1`,"
                        "\n\t\tSUBPARTITION `sp2`\n\t),\n\tPARTITION `p1` VALUES LESS THAN (200)""\n\t("
                        "\n\t\tSUBPARTITION `sp3`,\n\t\tSUBPARTITION `sp4`,"
                        "\n\t\tSUBPARTITION `sp5`\n\t)\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_17) {
  std::string source = "CREATE TABLE t2_f_lrc(col1 INT,col2 TIMESTAMP) \n" 
                        "       PARTITION BY LIST(col1)\n" 
                        "       SUBPARTITION BY RANGE COLUMNS(UNIX_TIMESTAMP(col2))\n" 
                        "        (PARTITION p0 VALUES IN(100)\n" 
                        "           (SUBPARTITION sp0 VALUES LESS THAN(UNIX_TIMESTAMP('2021/04/01')),\n" 
                        "            SUBPARTITION sp1 VALUES LESS THAN(UNIX_TIMESTAMP('2021/07/01')),\n" 
                        "            SUBPARTITION sp2 VALUES LESS THAN(UNIX_TIMESTAMP('2021/10/01')),\n" 
                        "            SUBPARTITION sp3 VALUES LESS THAN(UNIX_TIMESTAMP('2022/01/01'))\n" 
                        "           ),\n" 
                        "         PARTITION p1 VALUES IN(200)\n" 
                        "           (SUBPARTITION sp4 VALUES LESS THAN(UNIX_TIMESTAMP('2021/04/01')),\n" 
                        "            SUBPARTITION sp5 VALUES LESS THAN(UNIX_TIMESTAMP('2021/07/01')),\n" 
                        "            SUBPARTITION sp6 VALUES LESS THAN(UNIX_TIMESTAMP('2021/10/01')),\n" 
                        "            SUBPARTITION sp7 VALUES LESS THAN(UNIX_TIMESTAMP('2022/01/01'))\n" 
                        "            )\n" 
                        "         );";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_18) {
  std::string source = "CREATE TABLE t2_f_llc(col1 INT,col2 TIMESTAMP) \n" 
                        "       PARTITION BY LIST(col1)\n" 
                        "       SUBPARTITION BY LIST COLUMNS(UNIX_TIMESTAMP(col2))\n" 
                        "        (PARTITION p0 VALUES IN(100)\n" 
                        "           (SUBPARTITION sp0 VALUES IN(UNIX_TIMESTAMP('2021/04/01')),\n" 
                        "            SUBPARTITION sp1 VALUES IN(UNIX_TIMESTAMP('2021/07/01')),\n" 
                        "            SUBPARTITION sp2 VALUES IN(UNIX_TIMESTAMP('2021/10/01')),\n" 
                        "            SUBPARTITION sp3 VALUES IN(UNIX_TIMESTAMP('2022/01/01'))\n" 
                        "           ),\n" 
                        "         PARTITION p1 VALUES IN(200)\n" 
                        "           (SUBPARTITION sp4 VALUES IN(UNIX_TIMESTAMP('2021/04/01')),\n" 
                        "            SUBPARTITION sp5 VALUES IN(UNIX_TIMESTAMP('2021/07/01')),\n" 
                        "            SUBPARTITION sp6 VALUES IN(UNIX_TIMESTAMP('2021/10/01')),\n" 
                        "            SUBPARTITION sp7 VALUES IN(UNIX_TIMESTAMP('2022/01/01'))\n" 
                        "            )\n" 
                        "         );";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_19) {
  std::string source = "CREATE TABLE t2_f_lch (col1 INT NOT NULL,col2 varchar(50),col3 INT NOT NULL) \n" 
                        "PARTITION BY LIST COLUMNS(col1)\n" 
                        "SUBPARTITION BY HASH(col3)\n" 
                        "(PARTITION p0 VALUES IN(100)\n" 
                        "  (SUBPARTITION sp0,\n" 
                        "   SUBPARTITION sp1,\n" 
                        "   SUBPARTITION sp2),\n" 
                        " PARTITION p1 VALUES IN(200)\n" 
                        "  (SUBPARTITION sp3,\n" 
                        "   SUBPARTITION sp4,\n" 
                        "   SUBPARTITION sp5)\n" 
                        "); ";
  std::string expect = "CREATE TABLE `t2_f_lch`(\n\t`col1` INTEGER NOT NULL ,"
                        "\n\t`col2` VARCHAR (50),\n\t`col3` INTEGER NOT NULL \n)"
                        " PARTITION BY LIST COLUMNS (`col1`) SUBPARTITION BY HASH(`col3`)"
                        " \n(\n\tPARTITION `p0` VALUES IN(100)\n\t("
                        "\n\t\tSUBPARTITION `sp0`,\n\t\tSUBPARTITION `sp1`,"
                        "\n\t\tSUBPARTITION `sp2`\n\t),\n\tPARTITION `p1` VALUES IN(200)\n\t("
                        "\n\t\tSUBPARTITION `sp3`,\n\t\tSUBPARTITION `sp4`,"
                        "\n\t\tSUBPARTITION `sp5`\n\t)\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_WITH_PARTITION_20) {
  std::string source = "CREATE TABLE t2_f_hlc (col1 INT,col2 INT) \n" 
                        "       PARTITION BY HASH(col1) \n" 
                        "       SUBPARTITION BY LIST COLUMNS(col2)\n" 
                        "        (PARTITION p1\n" 
                        "          (SUBPARTITION sp0 VALUES IN (2020)\n" 
                        "          ,SUBPARTITION sp1 VALUES IN (2021)\n" 
                        "          ,SUBPARTITION sp2 VALUES IN (2022)\n" 
                        "          ,SUBPARTITION sp3 VALUES IN (2023)\n" 
                        "           ),\n" 
                        "         PARTITION p2\n" 
                        "          (SUBPARTITION sp4 VALUES IN (2020)\n" 
                        "          ,SUBPARTITION sp5 VALUES IN (2021)\n" 
                        "          ,SUBPARTITION sp6 VALUES IN (2022)\n" 
                        "          ,SUBPARTITION sp7 VALUES IN (2023)\n" 
                        "           )\n" 
                        "        );";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_WITH_DEFAULT_1) {
  std::string source = "create table t (\n"
                        "uctinyint   tinyint   unsigned default 255,   \n"
                        "ctinyint    tinyint            default -128,  \n"
                        "ucsmallint  smallint  unsigned default 65535, \n"
                        "csmallint   smallint           default -32768,\n"
                        "ucmediumint mediumint unsigned default 16777215, \n"
                        "cmediumint  mediumint          default -8388608, \n"
                        "cint        int                default -2147483648, \n"
                        "ucinteger   integer   unsigned default 4294967295, \n"
                        "cinteger    integer            default -2147483648, \n"
                        "ucbigint    bigint    unsigned default 18446744073709551615, \n"
                        "cbigint     bigint             default -9223372036854775808, \n"
                        "cfloat      float(10,2)        default 12345678.12, \n"
                        "cdouble     double             default 123.123, \n"
                        "cdec        dec(10,0)          default 123, \n"
                        "cnumeric    numeric            default 123.456, \n"
                        "cdecimal    decimal(13,2)      default 12345678901.12, \n"
                        "cbool       bool               default true, \n"
                        "cdatetime   datetime(6)        default current_timestamp(6), \n"
                        "ctimestamp  timestamp(3)       default now(3), \n"
                        "ctime       time               default '0:0:0', \n"
                        "`CTIME2` TIME DEFAULT time '0:0:0',\n"
                        "cdate       date               default '2020-12-01', \n"
                        "`CDATE2` DATE DEFAULT date '2020-12-01',\n"
                        "cyear       year               default 2020, \n"
                        "cchar       char(4)            default 'abcd', \n"
                        "ccharacter  character(8)       default 'abcdefgh', \n"
                        "cvarchar    varchar(16)        default 'abc', \n"
                        "ctinytext   tinytext           default null, \n"
                        "ctext       text               default null, \n"
                        "cmediumtext mediumtext         default null,\n"
                        "clongtext   longtext           default null, \n"
                        "ctinyblob   tinyblob           default null, \n"
                        "cblob       blob               default null, \n"
                        "cmediumblob mediumblob         default null, \n"
                        "clongblob   longblob           default null, \n"
                        "cbinary     binary(32)         default b'1110', \n"
                        "cvarbinary  varbinary(64)      default b'1111', \n"
                        "cbit        bit(64)            default b'1000',\n"
                        "`CBIT2`     BIT (64)           DEFAULT '1000',\n"
                        "cenum       enum('a','b','bc') default 'a', \n"
                        "ctimestamp2       timestamp(2) default localtime(2), \n"
                        "ctimestamp3       timestamp default localtime, \n"
                        "ctimestamp4       timestamp(3) default LOCALTIMESTAMP(3), \n"
                        "ctimestamp5       timestamp default LOCALTIMESTAMP, \n"
                        "ctimestamp6       timestamp default now(), \n"
                        "ctimestamp7       timestamp default current_timestamp(), \n"
                        "`CTIMESTAMP8` TIMESTAMP DEFAULT timestamp '2020-1-1 19:01:01',\n"
                        "cset        set('abc','b','abcde') default 'abc');";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`uctinyint` TINYINT UNSIGNED DEFAULT 255,\n"
                        "\t`ctinyint` TINYINT DEFAULT -128,\n"
                        "\t`ucsmallint` SMALLINT UNSIGNED DEFAULT 65535,\n"
                        "\t`csmallint` SMALLINT DEFAULT -32768,\n"
                        "\t`ucmediumint` MEDIUMINT UNSIGNED DEFAULT 16777215,\n"
                        "\t`cmediumint` MEDIUMINT DEFAULT -8388608,\n"
                        "\t`cint` INTEGER DEFAULT -2147483648,\n"
                        "\t`ucinteger` INTEGER UNSIGNED DEFAULT 4294967295,\n"
                        "\t`cinteger` INTEGER DEFAULT -2147483648,\n"
                        "\t`ucbigint` BIGINT UNSIGNED DEFAULT 18446744073709551615,\n"
                        "\t`cbigint` BIGINT DEFAULT -9223372036854775808,\n"
                        "\t`cfloat` FLOAT (10, 2) DEFAULT 12345678.12,\n"
                        "\t`cdouble` DOUBLE DEFAULT 123.123,\n"
                        "\t`cdec` DECIMAL (10, 0) DEFAULT 123,\n"
                        "\t`cnumeric` NUMERIC DEFAULT 123.456,\n"
                        "\t`cdecimal` DECIMAL (13, 2) DEFAULT 12345678901.12,\n"
                        "\t`cbool` BOOLEAN DEFAULT TRUE,\n"
                        "\t`cdatetime` DATETIME (6) DEFAULT CURRENT_TIMESTAMP(6),\n"
                        "\t`ctimestamp` TIMESTAMP (3) DEFAULT NOW(3) NULL ,\n"
                        "\t`ctime` TIME DEFAULT '0:0:0',\n"
                        "\t`CTIME2` TIME DEFAULT '0:0:0',\n"
                        "\t`cdate` DATE DEFAULT '2020-12-01',\n"
                        "\t`CDATE2` DATE DEFAULT '2020-12-01',\n"
                        "\t`cyear` YEAR DEFAULT 2020,\n"
                        "\t`cchar` CHAR (4) DEFAULT 'abcd',\n"
                        "\t`ccharacter` CHAR (8) DEFAULT 'abcdefgh',\n"
                        "\t`cvarchar` VARCHAR (16) DEFAULT 'abc',\n"
                        "\t`ctinytext` TINYTEXT DEFAULT NULL,\n"
                        "\t`ctext` TEXT DEFAULT NULL,\n"
                        "\t`cmediumtext` MEDIUMTEXT DEFAULT NULL,\n"
                        "\t`clongtext` LONGTEXT DEFAULT NULL,\n"
                        "\t`ctinyblob` TINYBLOB DEFAULT NULL,\n"
                        "\t`cblob` BLOB DEFAULT NULL,\n"
                        "\t`cmediumblob` MEDIUMBLOB DEFAULT NULL,\n"
                        "\t`clongblob` LONGBLOB DEFAULT NULL,\n"
                        "\t`cbinary` BINARY (32) DEFAULT b'1110',\n"
                        "\t`cvarbinary` VARBINARY (64) DEFAULT b'1111',\n"
                        "\t`cbit` BIT (64) DEFAULT b'1000',\n"
                        "\t`CBIT2` BIT (64) DEFAULT '1000',\n"
                        "\t`cenum` ENUM('a','b','bc') DEFAULT 'a',\n"
                        "\t`ctimestamp2` TIMESTAMP (2) DEFAULT LOCALTIME(2) NULL ,\n"
                        "\t`ctimestamp3` TIMESTAMP DEFAULT LOCALTIME() NULL ,\n"
                        "\t`ctimestamp4` TIMESTAMP (3) DEFAULT LOCALTIMESTAMP(3) NULL ,\n"
                        "\t`ctimestamp5` TIMESTAMP DEFAULT LOCALTIMESTAMP NULL ,\n"
                        "\t`ctimestamp6` TIMESTAMP DEFAULT NOW() NULL ,\n"
                        "\t`ctimestamp7` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL ,\n"
                        "\t`CTIMESTAMP8` TIMESTAMP DEFAULT '2020-1-1 19:01:01' NULL ,\n"
                        "\t`cset` SET('abc','b','abcde') DEFAULT 'abc'\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_2) {
  std::string source = "create table t2 (`c1` timestamp, primary key (`c1`))";
  std::string expect = "CREATE TABLE `t2`(\n"
                        "\t`c1` TIMESTAMP NOT NULL ,\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_3) {
  std::string source = "create table t2 (`c1` int, primary key (`c1`))";
  std::string expect = "CREATE TABLE `t2`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_4) {
  std::string source = "create table t2 (`c1` timestamp)";
  std::string expect = "CREATE TABLE `t2`(\n"
                        "\t`c1` TIMESTAMP NULL \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_5) {
  std::string source = "create table tt1(c1 int, c2 int, c3 int, c4 int, constraint t1_pk1 primary key(c1, c2), constraint t1_uk1 unique(c3, c4))";
  std::string expect = "CREATE TABLE `tt1`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\t`c2` INTEGER NOT NULL ,\n"
                        "\t`c3` INTEGER,\n"
                        "\t`c4` INTEGER,\n"
                        "\tPRIMARY KEY (`c1`,`c2`),\n"
                        "\tUNIQUE KEY `t1_uk1` (`c3`,`c4`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_6) {
  std::string source = "create table t1(c1 int, c2 char(32), primary key(c1));;;;;;";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\t`c2` CHAR (32),\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_INDEX_1) {
  std::string source = "CREATE TABLE `tms_failed_jobs` (\n"
                "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT COMMENT '主键',\n"
                "  `mq_connection` varchar(50) NOT NULL DEFAULT '' COMMENT '队列连接信息名',\n"
                "  `queue` varchar(50) NOT NULL DEFAULT '' COMMENT '队列名',\n"
                "  `payload` longtext COMMENT '消息结构',\n"
                "  `exception` text COMMENT '异常信息',\n"
                "  `failed_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '失败时间',\n"
                "  `deleted_at` datetime DEFAULT NULL COMMENT '删除时间',\n"
                "  `insert_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',\n"
                "  `last_update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',\n"
                "  PRIMARY KEY (`id`) USING BTREE,\n"
                "  KEY `idx_mq_connection` (`mq_connection`),\n"
                "  KEY `idx_queue` (`queue`),\n"
                "  KEY `idx_failed_at` (`failed_at`),\n"
                "  KEY `idx_last_update_time` (`last_update_time`)\n"
                ") ENGINE=InnoDB AUTO_INCREMENT=1963 DEFAULT CHARSET=utf8mb4 COMMENT='队列异常补偿表';";
  std::string expect = "CREATE TABLE `tms_failed_jobs`(\n"
                "\t`id` BIGINT (20) UNSIGNED NOT NULL  AUTO_INCREMENT  COMMENT '主键',\n"
                "\t`mq_connection` VARCHAR (50) DEFAULT '' NOT NULL  COMMENT '队列连接信息名',\n"
                "\t`queue` VARCHAR (50) DEFAULT '' NOT NULL  COMMENT '队列名',\n"
                "\t`payload` LONGTEXT COMMENT '消息结构',\n"
                "\t`exception` TEXT COMMENT '异常信息',\n"
                "\t`failed_at` DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL  COMMENT '失败时间',\n"
                "\t`deleted_at` DATETIME DEFAULT NULL COMMENT '删除时间',\n"
                "\t`insert_time` DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL  COMMENT '创建时间',\n"
                "\t`last_update_time` DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL  ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',\n"
                "\tPRIMARY KEY (`id`),\n"
                "\tINDEX `idx_mq_connection`(`mq_connection`) ,\n"
                "\tINDEX `idx_queue`(`queue`) ,\n"
                "\tINDEX `idx_failed_at`(`failed_at`) ,\n"
                "\tINDEX `idx_last_update_time`(`last_update_time`) \n"
                ") CHARACTER SET = utf8mb4 COMMENT '队列异常补偿表'";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_VARCHAR) {
  std::string source = "CREATE TABLE `obschema_table_stat`(\n"
                "  `tenant_name` VARCHAR (64) NOT NULL ,\n"
                "  `database_name` VARCHAR (64) NOT NULL ,\n"
                "  `table_name` VARCHAR (64) NOT NULL ,\n"
                "  `json` VARCHAR (65536) DEFAULT NULL,\n"
                "  `cnt` INTEGER (11) DEFAULT NULL,\n"
                "  `gmt_create` DATETIME (6) DEFAULT CURRENT_TIMESTAMP(6),\n"
                "  `gmt_modified` DATETIME (6) DEFAULT CURRENT_TIMESTAMP(6),\n"
                "  PRIMARY KEY (`tenant_name`,`database_name`,`table_name`)\n"
                ");\n";
  std::string expect = "CREATE TABLE `obschema_table_stat`(\n"
                "\t`tenant_name` VARCHAR (64) NOT NULL ,\n"
                "\t`database_name` VARCHAR (64) NOT NULL ,\n"
                "\t`table_name` VARCHAR (64) NOT NULL ,\n"
                "\t`json` MEDIUMTEXT DEFAULT NULL,\n"
                "\t`cnt` INTEGER (11) DEFAULT NULL,\n"
                "\t`gmt_create` DATETIME (6) DEFAULT CURRENT_TIMESTAMP(6),\n"
                "\t`gmt_modified` DATETIME (6) DEFAULT CURRENT_TIMESTAMP(6),\n"
                "\tPRIMARY KEY (`tenant_name`,`database_name`,`table_name`)\n"
                ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_7) {
  std::string source = "CREATE TABLE `sub_listkey` (\n"
                        "  `userid` int(11) NOT NULL DEFAULT '0',\n"
                        "  `usertype` int(11) NOT NULL DEFAULT '0',\n"
                        "  PRIMARY KEY (`userid`, `usertype`)\n"
                        ") DEFAULT CHARSET = utf8mb4 ROW_FORMAT = DYNAMIC COMPRESSION = 'zstd_1.0' REPLICA_NUM = 1 BLOCK_SIZE = 16384 USE_BLOOM_FILTER = FALSE TABLET_SIZE = 134217728 PCTFREE = 0\n"
                        "partition by list(usertype) subpartition by key(`userid`) subpartition template (\n"
                        "subpartition p0,\n"
                        "subpartition p1,\n"
                        "subpartition p2)\n"
                        "(partition p1 values in (1),\n"
                        "partition p2 values in (2),\n"
                        "partition p3 values in (3))";
  std::string expect = "CREATE TABLE `sub_listkey`(\n"
                        "\t`userid` INTEGER (11) DEFAULT '0' NOT NULL ,\n"
                        "\t`usertype` INTEGER (11) DEFAULT '0' NOT NULL ,\n"
                        "\tPRIMARY KEY (`userid`,`usertype`)\n"
                        ") CHARACTER SET = utf8mb4";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_8) {
  std::string source = "CREATE TABLE `partrange_list_nopri` (\n"
                        "  `col1` int(11) DEFAULT NULL,\n"
                        "  `col2` decimal(10,2) DEFAULT NULL,\n"
                        "  `col3` decimal(10,2) DEFAULT NULL,\n"
                        "  `col4` bit(1) DEFAULT NULL,\n"
                        "  `col5` tinyint(4) DEFAULT NULL,\n"
                        "  `col6` smallint(6) DEFAULT NULL,\n"
                        "  `col7` mediumint(9) DEFAULT NULL,\n"
                        "  `col8` bigint(20) DEFAULT NULL,\n"
                        "  `col9` float(10,2) DEFAULT NULL,\n"
                        "  `col10` double(10,2) DEFAULT NULL,\n"
                        "  `col11` varchar(10) DEFAULT NULL,\n"
                        "  `col12` char(10) DEFAULT NULL,\n"
                        "  `col13` text DEFAULT NULL,\n"
                        "  `col14` tinytext DEFAULT NULL,\n"
                        "  `col15` mediumtext DEFAULT NULL,\n"
                        "  `col16` longtext DEFAULT NULL,\n"
                        "  `col17` blob DEFAULT NULL,\n"
                        "  `col18` tinyblob DEFAULT NULL,\n"
                        "  `clo19` longblob DEFAULT NULL,\n"
                        "  `col20` mediumblob DEFAULT NULL,\n"
                        "  `col21` binary(16) DEFAULT NULL,\n"
                        "  `col22` varbinary(16) DEFAULT NULL,\n"
                        "  `col23` timestamp NULL DEFAULT NULL,\n"
                        "  `col24` time DEFAULT NULL,\n"
                        "  `col25` date DEFAULT NULL,\n"
                        "  `col26` datetime DEFAULT NULL,\n"
                        "  `col27` year(4) DEFAULT NULL,\n"
                        "  `OMS_PK_INCRMT` bigint(20) DEFAULT NULL,\n"
                        "  UNIQUE KEY `UK_partrange_list_nopri_OBPK_INCRMT` (`col6`, `col1`, `OMS_PK_INCRMT`) BLOCK_SIZE 16384 LOCAL\n"
                        ") DEFAULT CHARSET = utf8mb4 ROW_FORMAT = DYNAMIC COMPRESSION = 'zstd_1.0' REPLICA_NUM = 1 BLOCK_SIZE = 16384 USE_BLOOM_FILTER = FALSE TABLET_SIZE = 134217728 PCTFREE = 0\n"
                        "partition by range(col6) subpartition by list(col1) subpartition template (\n"
                        "subpartition p_1 values in (1),\n"
                        "subpartition p_2 values in (2),\n"
                        "subpartition p_3 values in (3),\n"
                        "subpartition p_4 values in (4))\n"
                        "(partition p1 values less than (0),\n"
                        "partition p2 values less than (10000),\n"
                        "partition p3 values less than (100000),\n"
                        "partition p4 values less than (MAXVALUE))";
  std::string expect = "CREATE TABLE `partrange_list_nopri`(\n"
                        "\t`col1` INTEGER (11) DEFAULT NULL,\n"
                        "\t`col2` DECIMAL (10, 2) DEFAULT NULL,\n"
                        "\t`col3` DECIMAL (10, 2) DEFAULT NULL,\n"
                        "\t`col4` BIT (1) DEFAULT NULL,\n"
                        "\t`col5` TINYINT (4) DEFAULT NULL,\n"
                        "\t`col6` SMALLINT (6) DEFAULT NULL,\n"
                        "\t`col7` MEDIUMINT (9) DEFAULT NULL,\n"
                        "\t`col8` BIGINT (20) DEFAULT NULL,\n"
                        "\t`col9` FLOAT (10, 2) DEFAULT NULL,\n"
                        "\t`col10` DOUBLE (10, 2) DEFAULT NULL,\n"
                        "\t`col11` VARCHAR (10) DEFAULT NULL,\n"
                        "\t`col12` CHAR (10) DEFAULT NULL,\n"
                        "\t`col13` TEXT DEFAULT NULL,\n"
                        "\t`col14` TINYTEXT DEFAULT NULL,\n"
                        "\t`col15` MEDIUMTEXT DEFAULT NULL,\n"
                        "\t`col16` LONGTEXT DEFAULT NULL,\n"
                        "\t`col17` BLOB DEFAULT NULL,\n"
                        "\t`col18` TINYBLOB DEFAULT NULL,\n"
                        "\t`clo19` LONGBLOB DEFAULT NULL,\n"
                        "\t`col20` MEDIUMBLOB DEFAULT NULL,\n"
                        "\t`col21` BINARY (16) DEFAULT NULL,\n"
                        "\t`col22` VARBINARY (16) DEFAULT NULL,\n"
                        "\t`col23` TIMESTAMP DEFAULT NULL NULL ,\n"
                        "\t`col24` TIME DEFAULT NULL,\n"
                        "\t`col25` DATE DEFAULT NULL,\n"
                        "\t`col26` DATETIME DEFAULT NULL,\n"
                        "\t`col27` YEAR (4) DEFAULT NULL,\n"
                        "\t`OMS_PK_INCRMT` BIGINT (20) DEFAULT NULL,\n"
                        "\tUNIQUE KEY `UK_partrange_list_nopri_OBPK_INCRMT` (`col6`,`col1`,`OMS_PK_INCRMT`)\n"
                        ") CHARACTER SET = utf8mb4";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_9) {
  std::string source = "CREATE TABLE test_comment_partition( \n"
                "    c1 BIGINT PRIMARY KEY, \n"
                "    c2 VARCHAR(50)\n"
                ") COMMENT = '测试表' PARTITION BY LIST(c1) \n"
                "    ( \n"
                "    PARTITION p0    VALUES IN (1, 2, 3), \n"
                "    PARTITION p1  VALUES IN (5, 6), \n"
                "    PARTITION p2  VALUES IN (DEFAULT)\n"
                " );";
  std::string expect = "CREATE TABLE `test_comment_partition`(\n"
                "\t`c1` BIGINT NOT NULL ,\n"
                "\t`c2` VARCHAR (50),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ")  COMMENT '测试表'";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_10) {
  std::string source = "create table t(\n"
                "\tcA int, \n"
                "\tcb int unsigned, \n"
                "\t`c2` char(32), \n"
                "\t`Ct` timestamp, \n"
                "\tprimary key(`c2`,cA))";
  std::string expect = "CREATE TABLE `t`(\n"
                "\t`cA` INTEGER NOT NULL ,\n"
                "\t`cb` INTEGER UNSIGNED,\n"
                "\t`c2` CHAR (32) NOT NULL ,\n"
                "\t`Ct` TIMESTAMP NULL ,\n"
                "\tPRIMARY KEY (`c2`,`cA`)\n"
                ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}


TEST(CREATE_TABLE, CREATE_TABLE_DEFAULT_1) {
  std::string source = "create table t1 (c1 binary(10) default 0xa, c2 binary(10) default x'a');";
  std::string expect = "CREATE TABLE `t1`(\n\t`c1` BINARY (10) DEFAULT 0xa,\n\t`c2` BINARY (10) DEFAULT 0xa\n)";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_7) {
  std::string source = "create table t1(c1 int check(c1 > 0) check(c1 > 1))";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER\tCHECK (`c1` > 1 ) \tCHECK (`c1` > 0 ) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_CONSTRAINTS_8) {
  std::string source = "create table t(c1 int constraint a check(c1 > 0) constraint b check(c1 > 1))";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`c1` INTEGER\tCONSTRAINT `b` CHECK (`c1` > 1 ) \tCONSTRAINT `a` CHECK (`c1` > 0 ) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_1) {
  std::string source = "ALTER TABLE dint_pk12 MODIFY COLUMN `c011` VARCHAR(21845);";
  std::string expect = "ALTER TABLE `dint_pk12`\n\tMODIFY `c011` TEXT\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_2) {
  std::string source = "alter table t1 add column c1 int check(c1 > 10);";
  std::string expect = "ALTER TABLE `t1`\n\t\tADD COLUMN `c1` INTEGER\tCHECK (`c1` > 10 ) \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_3) {
  std::string source = "alter table t1 add constraint a check(c1 > 10);";
  std::string expect = "ALTER TABLE `t1`\n\t\tADD CONSTRAINT `a` CHECK (`c1` > 10 )\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_4) {
  std::string source = "alter table t_test_01 add column smallint_data_type smallint(4294967295) unsigned;";
  std::string expect = "ALTER TABLE `t_test_01`\n"
                        "\t\tADD COLUMN `smallint_data_type` SMALLINT (255) UNSIGNED\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_5) {
  std::string source = "alter table t add column c1 INTEGER not null default 1 comment 'add column';";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t\tADD COLUMN `c1` INTEGER DEFAULT 1 NOT NULL  COMMENT 'add column'\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_6) {
  std::string source = "ALTER TABLE `T`\n"
                        "\t\tADD COLUMN `UCTINYINT` TINYINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CTINYINT` TINYINT, \n"
                        "\t\tADD COLUMN `UCSMALLINT` SMALLINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CSMALLINT` SMALLINT, \n"
                        "\t\tADD COLUMN `CMEDIUMINT` MEDIUMINT, \n"
                        "\t\tADD COLUMN `CINT` INTEGER, \n"
                        "\t\tADD COLUMN `UCINTEGER` INTEGER UNSIGNED, \n"
                        "\t\tADD COLUMN `CINTEGER` INTEGER, \n"
                        "\t\tADD COLUMN `UCBIGINT` BIGINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CBIGINT` BIGINT, \n"
                        "\t\tADD COLUMN `CFLOAT` FLOAT (10, 2), \n"
                        "\t\tADD COLUMN `CDOUBLE` DOUBLE, \n"
                        "\t\tADD COLUMN `CDEC` DECIMAL (10, 0), \n"
                        "\t\tADD COLUMN `CNUMERIC` NUMERIC, \n"
                        "\t\tADD COLUMN `CDECIMAL` DECIMAL (13, 2), \n"
                        "\t\tADD COLUMN `CBOOL` BOOLEAN, \n"
                        "\t\tADD COLUMN `CDATETIME` DATETIME (6), \n"
                        "\t\tADD COLUMN `CTIMESTAMP` TIMESTAMP (3) NOT NULL , \n"
                        "\t\tADD COLUMN `CTIME` TIME, \n"
                        "\t\tADD COLUMN `CDATE` DATE, \n"
                        "\t\tADD COLUMN `CYEAR` YEAR, \n"
                        "\t\tADD COLUMN `CCHAR` CHAR (4), \n"
                        "\t\tADD COLUMN `CCHARACTER` CHAR (8), \n"
                        "\t\tADD COLUMN `CVARCHAR` VARCHAR (16), \n"
                        "\t\tADD COLUMN `CTINYTEXT` TINYTEXT, \n"
                        "\t\tADD COLUMN `CTEXT` TEXT, \n"
                        "\t\tADD COLUMN `CMEDIUMTEXT` MEDIUMTEXT, \n"
                        "\t\tADD COLUMN `CLONGTEXT` LONGTEXT, \n"
                        "\t\tADD COLUMN `CTINYBLOB` TINYBLOB, \n"
                        "\t\tADD COLUMN `CBLOB` BLOB, \n"
                        "\t\tADD COLUMN `CMEDIUMBLOB` MEDIUMBLOB, \n"
                        "\t\tADD COLUMN `CLONGBLOB` LONGBLOB, \n"
                        "\t\tADD COLUMN `CBINARY` BINARY (32), \n"
                        "\t\tADD COLUMN `CVARBINARY` VARBINARY (64), \n"
                        "\t\tADD COLUMN `CBIT` BIT (64), \n"
                        "\t\tADD COLUMN `CENUM` ENUM('a','b','bc'), \n"
                        "\t\tADD COLUMN `CSET` SET('abc','b','abcde')";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t\tADD COLUMN `UCTINYINT` TINYINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CTINYINT` TINYINT, \n"
                        "\t\tADD COLUMN `UCSMALLINT` SMALLINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CSMALLINT` SMALLINT, \n"
                        "\t\tADD COLUMN `CMEDIUMINT` MEDIUMINT, \n"
                        "\t\tADD COLUMN `CINT` INTEGER, \n"
                        "\t\tADD COLUMN `UCINTEGER` INTEGER UNSIGNED, \n"
                        "\t\tADD COLUMN `CINTEGER` INTEGER, \n"
                        "\t\tADD COLUMN `UCBIGINT` BIGINT UNSIGNED, \n"
                        "\t\tADD COLUMN `CBIGINT` BIGINT, \n"
                        "\t\tADD COLUMN `CFLOAT` FLOAT (10, 2), \n"
                        "\t\tADD COLUMN `CDOUBLE` DOUBLE, \n"
                        "\t\tADD COLUMN `CDEC` DECIMAL (10, 0), \n"
                        "\t\tADD COLUMN `CNUMERIC` NUMERIC, \n"
                        "\t\tADD COLUMN `CDECIMAL` DECIMAL (13, 2), \n"
                        "\t\tADD COLUMN `CBOOL` BOOLEAN, \n"
                        "\t\tADD COLUMN `CDATETIME` DATETIME (6), \n"
                        "\t\tADD COLUMN `CTIMESTAMP` TIMESTAMP (3) NOT NULL , \n"
                        "\t\tADD COLUMN `CTIME` TIME, \n"
                        "\t\tADD COLUMN `CDATE` DATE, \n"
                        "\t\tADD COLUMN `CYEAR` YEAR, \n"
                        "\t\tADD COLUMN `CCHAR` CHAR (4), \n"
                        "\t\tADD COLUMN `CCHARACTER` CHAR (8), \n"
                        "\t\tADD COLUMN `CVARCHAR` VARCHAR (16), \n"
                        "\t\tADD COLUMN `CTINYTEXT` TINYTEXT, \n"
                        "\t\tADD COLUMN `CTEXT` TEXT, \n"
                        "\t\tADD COLUMN `CMEDIUMTEXT` MEDIUMTEXT, \n"
                        "\t\tADD COLUMN `CLONGTEXT` LONGTEXT, \n"
                        "\t\tADD COLUMN `CTINYBLOB` TINYBLOB, \n"
                        "\t\tADD COLUMN `CBLOB` BLOB, \n"
                        "\t\tADD COLUMN `CMEDIUMBLOB` MEDIUMBLOB, \n"
                        "\t\tADD COLUMN `CLONGBLOB` LONGBLOB, \n"
                        "\t\tADD COLUMN `CBINARY` BINARY (32), \n"
                        "\t\tADD COLUMN `CVARBINARY` VARBINARY (64), \n"
                        "\t\tADD COLUMN `CBIT` BIT (64), \n"
                        "\t\tADD COLUMN `CENUM` ENUM('a','b','bc'), \n"
                        "\t\tADD COLUMN `CSET` SET('abc','b','abcde')\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_7) {
  std::string source = "    ALTER TABLE `MYTABLE`\n"
                        "    MODIFY `FOO` VARCHAR (32) NOT NULL  AFTER `BAZ`";
  std::string expect = "ALTER TABLE `mytable`\n"
                        "\tMODIFY `FOO` VARCHAR (32) NOT NULL  AFTER `BAZ`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_8) {
  std::string source = "ALTER TABLE `MYTABLE` ALTER COLUMN `FOO` SET DEFAULT 'bar'";
  std::string expect = "ALTER TABLE `mytable`\n"
                        "\tALTER COLUMN `FOO` SET DEFAULT 'bar'\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_9) {
  std::string source = "ALTER TABLE `MYTABLE` ALTER COLUMN `FOO` DROP DEFAULT";
  std::string expect = "ALTER TABLE `mytable`\n"
                        "\tALTER COLUMN `FOO` DROP DEFAULT \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_10) {
  std::string source = "alter table t drop column c1;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\tDROP COLUMN `c1`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_11) {
  std::string source = "ALTER TABLE `T1`\n"
                        "\tCHANGE COLUMN `C001`\t`D001` VARCHAR (64) COMMENT 'change column', \n"
                        "\tCHANGE COLUMN `C002`\t`D002` BIGINT DEFAULT 100 NOT NULL  COMMENT 'change column'";
  std::string expect = "ALTER TABLE `t1`\n"
                        "\tCHANGE COLUMN `C001`\t`D001` VARCHAR (64) COMMENT 'change column', \n"
                        "\tCHANGE COLUMN `C002`\t`D002` BIGINT DEFAULT 100 NOT NULL  COMMENT 'change column'\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_12) {
  std::string source = "ALTER TABLE `int_pk` ADD n01 INT,CHANGE c01 m01 CHAR(35);";
  std::string expect = "ALTER TABLE `int_pk`\n"
                        "\t\tADD COLUMN `n01` INTEGER, \n"
                        "\tCHANGE COLUMN `c01`\t`m01` CHAR (35)\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_13) {
  std::string source = "alter table t1 add index idx (c1);";
  std::string expect = "ALTER TABLE `t1`\n"
                        "\t\tADD INDEX `idx`(`c1`) \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_14) {
  std::string source = "alter table t1 drop index idx;";
  std::string expect = "ALTER TABLE `t1`\n"
                        "\tDROP INDEX `idx`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_15) {
  std::string source = "alter table testiq8z7y add column c7 timestamp;";
  std::string expect = "ALTER TABLE `testiq8z7y`\n"
                        "\t\tADD COLUMN `c7` TIMESTAMP NULL \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_16) {
  std::string source = "alter table testiq8z7y add column c7 datetime;";
  std::string expect = "ALTER TABLE `testiq8z7y`\n"
                        "\t\tADD COLUMN `c7` DATETIME\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_17) {
  std::string source = "alter table testiq8z7y add column c7 timestamp(3) default current_timestamp;";
  std::string expect = "ALTER TABLE `testiq8z7y`\n"
                        "\t\tADD COLUMN `c7` TIMESTAMP (3) DEFAULT CURRENT_TIMESTAMP NULL \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_18) {
  std::string source = "alter table t_student2osftr change name name char(20);";
  std::string expect = "ALTER TABLE `t_student2osftr`\n"
                        "\tCHANGE COLUMN `name`\t`name` CHAR (20)\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_19) {
  std::string source = "alter table at1axsxs0 change c3 column3 int not null default 30;";
  std::string expect = "ALTER TABLE `at1axsxs0`\n"
                        "\tCHANGE COLUMN `c3`\t`column3` INTEGER DEFAULT 30 NOT NULL \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_20) {
  std::string source = "alter table .test add c1 int";
  std::string expect = "ALTER TABLE `test`\n\t\tADD COLUMN `c1` INTEGER\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_21) {
  std::string source = "alter table t1 modify column c2 int comment 'c2' after c1;";
  std::string expect = "ALTER TABLE `t1`\n\tMODIFY `c2` INTEGER COMMENT 'c2' AFTER `c1`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_22) {
  std::string source = "alter table t1 change column c2 c2 int comment 'c2' after c1;";
  std::string expect = "ALTER TABLE `t1`\n\tCHANGE COLUMN `c2`\t`c2` INTEGER COMMENT 'c2' AFTER `c1`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_23) {
  std::string source = "ALTER TABLE wip_history RENAME INDEX station_code_2 TO idx_station_code";
  std::string expect = "ALTER TABLE `wip_history`\n"
                        "\tRENAME INDEX `station_code_2` TO `idx_station_code`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_24) {
  std::string source = "alter table orders PARTITION BY HASH(order_id) PARTITIONS 3;";
  std::string expect = "ALTER TABLE `orders`\n"
                        "\t PARTITION BY HASH (`order_id`) PARTITIONS 3\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_25) {
  std::string source = "ALTER TABLE ORDERS PARTITION BY LIST(ORDER_ID) "
                        "(PARTITION p1 VALUES IN (0,1,2,3),"
                        "PARTITION p2 VALUES IN (10,100,1000,10000),"
                        "PARTITION p3 VALUES IN (100000,1000000))";
  std::string expect = "ALTER TABLE `orders`\n"
                        "\t PARTITION BY LIST (`ORDER_ID`) \n"
                        "(\n"
                        "\tPARTITION `p1` VALUES IN(0, 1, 2, 3),\n"
                        "\tPARTITION `p2` VALUES IN(10, 100, 1000, 10000),\n"
                        "\tPARTITION `p3` VALUES IN(100000, 1000000)\n"
                        ")\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_26) {
  std::string source = "ALTER TABLE ORDERS PARTITION BY RANGE(ORDER_ID) "
                        "(PARTITION p1 VALUES LESS THAN (10),"
                        "PARTITION p2 VALUES LESS THAN (1000),"
                        "PARTITION p3 VALUES LESS THAN (1000000),"
                        "PARTITION p4 VALUES LESS THAN (maxvalue))";
  std::string expect = "ALTER TABLE `orders`\n"
                        "\t PARTITION BY RANGE (`ORDER_ID`) \n"
                        "(\n"
                        "\tPARTITION `p1` VALUES LESS THAN (10),\n"
                        "\tPARTITION `p2` VALUES LESS THAN (1000),\n"
                        "\tPARTITION `p3` VALUES LESS THAN (1000000),\n"
                        "\tPARTITION `p4` VALUES LESS THAN (maxvalue)\n"
                        ")\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_27) {
  std::string source = "ALTER TABLE ORDERS PARTITION BY RANGE(C1) SUBPARTITION BY RANGE(C2)"
                        "(PARTITION p1 VALUES LESS THAN (10)(SUBPARTITION `p_1` VALUES LESS THAN (maxvalue)),"
                        "PARTITION p2 VALUES LESS THAN (1000),"
                        "PARTITION p3 VALUES LESS THAN (1000000),"
                        "PARTITION p4 VALUES LESS THAN (maxvalue))";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_28) {
  std::string source = "alter table t truncate partition p0,p1;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t TRUNCATE  PARTITION `p0`,`p1`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_29) {
  std::string source = "alter table t truncate partition p0;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t TRUNCATE  PARTITION `p0`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_30) {
  std::string source = "alter table t truncate subpartition p0,p1;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_31) {
  std::string source = "alter table t drop partition p0,p1;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t DROP  PARTITION `p0`,`p1`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_32) {
  std::string source = "alter table t drop partition p0;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t DROP  PARTITION `p0`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_33) {
  std::string source = "alter table t drop subpartition sp0,sp1;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_34) {
  std::string source = "alter table t drop check `c`";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_35) {
  std::string source = "alter table t drop constraint `c`";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_36) {
  std::string source = "alter table t drop constraint (c)";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_37) {
  std::string source = "alter table t add column newC int, drop partition p0;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t\tADD COLUMN `newC` INTEGER\n"
                        "\n"
                        "ALTER TABLE `t`\n"
                        "\t DROP  PARTITION `p0`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_38) {
  std::string source = "alter table t add column newC int, truncate partition p0;";
  std::string expect = "ALTER TABLE `t`\n"
                        "\t\tADD COLUMN `newC` INTEGER\n"
                        "\n"
                        "ALTER TABLE `t`\n"
                        "\t TRUNCATE  PARTITION `p0`\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_39) {
  std::string source = "alter table t1 change column c2 c2 int comment 'c2' before c1;";
  std::string expect = "ALTER TABLE `t1`\n"
                        "\tCHANGE COLUMN `c2`\t`c2` INTEGER COMMENT 'c2'\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_40) {
  std::string source = "alter table tt1 change column c2 c2 int first;";
  std::string expect = "ALTER TABLE `tt1`\n"
                        "\tCHANGE COLUMN `c2`\t`c2` INTEGER FIRST\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_41) {
  std::string source = "ALTER TABLE t_recommend_end_poi ADD PARTITION ( partition p20230821 values less than(20230822))";
  std::string expect = "ALTER TABLE `t_recommend_end_poi`\n"
                        "\tADD PARTITION (\n"
                        "\tPARTITION `p20230821` VALUES LESS THAN (20230822))\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_42) {
  std::string source = "ALTER TABLE t_recommend_end_poi ADD PARTITION ( partition p20230821 values less than('20230822'))";
  std::string expect = "ALTER TABLE `t_recommend_end_poi`\n"
                        "\tADD PARTITION (\n"
                        "\tPARTITION `p20230821` VALUES LESS THAN ('20230822'))\n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(RENAME_TABLE, RENAME_TABLE) {
  std::string source = "rename table t1 to t10, test2.t2 to test2.t20, test3.t3 to t30";
  std::string expect = "RENAME TABLE \n"
                        "\t`t1` TO `t10`,\n"
                        "\t`t2` TO `t20`,\n"
                        "\t`t3` TO `t30`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_1) {
  std::string source = "create index idx1 on t1(c2, c1);";
  std::string expect = "\tCREATE INDEX `idx1` ON `t1`(`c2`,`c1`) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_2) {
  std::string source = "create index idx1 on t1(c2 asc, c1 desc);";
  std::string expect = "\tCREATE INDEX `idx1` ON `t1`(`c2` ASC,`c1` DESC) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_3) {
  std::string source = "CREATE INDEX hehe01 ON `dint_pk1` (c02);";
  std::string expect = "\tCREATE INDEX `hehe01` ON `dint_pk1`(`c02`) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_4) {
  std::string source = "CREATE UNIQUE INDEX hehe1011 ON dint_pk1 (c02);";
  std::string expect = "\tCREATE UNIQUE INDEX `hehe1011` ON `dint_pk1`(`c02`) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_5) {
  std::string source = "create index idx_c117uq9i on t1rcy7rf(c1) partition by hash(c1) partitions 3;";
  std::string expect = "\tCREATE INDEX `idx_c117uq9i` ON `t1rcy7rf`(`c1`) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_6) {
  std::string source = "create index idx2 on t1(c2(8),c1(2),c3)";
  std::string expect = "\tCREATE INDEX `idx2` ON `t1`(`c2`(8),`c1`(2),`c3`) ";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_INDEX, CREATE_INDEX_7) {
  std::string source = "create index idx1 on t1(c2, c1) global comment 'idx1';";
  std::string expect = "\tCREATE INDEX `idx1` ON `t1`(`c2`,`c1`)  COMMENT 'idx1'";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(DROP_INDEX, DROP_INDEX_1) {
  std::string source = "drop index idx on t1;";
  std::string expect = "DROP INDEX `idx` ON `t1`";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_1) {
  std::string source = "create table t1(c1 int, c2 int as (c1 + 1), c3 int as (c2 + c1), "
                        "c4 date as (DATE_ADD('2008-01-02', INTERVAL 31 DAY)),"
                        "c5 time as (ADDDATE('2008-01-02', 31)),"
                        "c6 timestamp as (CURRENT_TIMESTAMP),"
                        "c7 date as (DATE('2003-12-31 01:02:03')),"
                        "c8 timestamp as (LOCALTIME()),"
                        "c9 timestamp as (LOCALTIMESTAMP()),"
                        "c10 timestamp as (now()) NULL,"
                        "c11 date as (DATE_SUB('2008-01-02', INTERVAL 31 DAY)),"
                        "c12 timestamp(6) as (SUBTIME('2007-12-31 23:59:59.999999','1 1:1:1.000002')),"
                        "c13 timestamp as (TIMESTAMP('2003-12-31')),"
                        "c14 date as (MAKEDATE(2011,31)),"
                        "c15 int as (DATEDIFF('2007-12-31 23:59:59','2007-12-30')),"
                        "c16 time as (MAKETIME(12,15,30)),"
                        "c17 time as (TIME('2003-12-31 01:02:03')),"
                        "c18 time as (TIMEDIFF('2000:01:01 00:00:00','2000:01:01 00:00:00.000001')),"
                        "c19 int as (YEAR('1987-01-01')),"
                        "c20 int as (ABS(2)),"
                        "c21 int as (CHAR_LENGTH('aaaaaa')),"
                        "c22 int as (FIELD('Bb', 'Aa', 'Bb', 'Cc', 'Dd', 'Ff')),"
                        "c23 int as (FIND_IN_SET('b','a,b,c,d')),"
                        "c24 int as (LOCATE('bar', 'foobarbar')),"
                        "c25 int as (STRCMP('text', 'text2')),"
                        "c26 char(20) as (FORMAT(12332.123456, 4)),"
                        "c27 int as (POSITION('12'  IN  '1234')),"
                        "c29 int as (DAYOFMONTH('2007-02-03')),"
                        "c30 int as (DAYOFWEEK('2007-02-03')),"
                        "c31 int as (DAYOFYEAR('2007-02-03')),"
                        "c32 int as (HOUR('10:05:03')),"
                        "c33 int as (MINUTE('2008-02-03 10:05:03')),"
                        "c34 int as (MONTH('2008-02-03')),"
                        "c35 int as (PERIOD_DIFF(200802,200703)),"
                        "c36 int as (QUARTER('2008-04-01')),"
                        "c37 int as (SECOND('10:05:03')),"
                        "c38 int as (TO_DAYS(950501)),"
                        "c39 char(20) as (CONCAT('My', 'S', 'QL')),"
                        "c40 char(40) as (CONCAT_WS(',','First name','Second name','Last Name')),"
                        "c41 char(20) as (INSERT('Quadratic', 3, 4, 'What')),"
                        "c42 char(20) as (LEFT('foobarbar', 5)),"
                        "c43 char(20) as (LOWER('QUADRATICALLY')),"
                        "c44 char(20) as (LPAD('hi',4,'\?\?')),"
                        "c45 char(20) as (RPAD('hi',5,'?')),"
                        "c46 char(20) as (LTRIM('  barbar')),"
                        "c47 char(20) as (RTRIM('barbar   ')),"
                        "c48 char(20) as (REPEAT('MySQL', 3)),"
                        "c49 char(20) as (REPLACE('www.****.com', 'w', 'Ww')),"
                        "c50 char(20) as (REVERSE('abc')),"
                        "c51 char(20) as (RIGHT('foobarbar', 4)),"
                        "c52 char(20) as (SPACE(6)),"
                        "c53 char(20) as (SUBSTRING('foobarbar' FROM 4)),"
                        "c54 char(20) as (SUBSTRING('Quadratically',5)),"
                        "c55 char(20) as (SUBSTRING('Quadratically',5,6)),"
                        "c55 char(20) as (SUBSTRING('Sakila', -3)),"
                        "c56 char(20) as (SUBSTRING('Sakila', -5, 3)),"
                        "c57 char(20) as (SUBSTRING('Sakila' FROM -4 FOR 2)),"
                        "c58 char(20) as (TRIM('  bar   ')),"
                        "c59 char(20) as (TRIM(LEADING 'x' FROM 'xxxbarxxx')),"
                        "c60 char(20) as (TRIM(BOTH 'x' FROM 'xxxbarxxx')),"
                        "c61 char(20) as (TRIM(TRAILING 'xyz' FROM 'barxxyz')),"
                        "c62 char(20) as (UPPER('aaaa')),"
                        "c63 int as (SQRT(4)),"
                        "c64 int as (3+5),"
                        "c65 int as (3-5),"
                        "c66 int as (-5),"
                        "c67 int as (3*5),"
                        "c68 int as (10/2),"
                        "c69 int as (10%2),"
                        "c70 int as (5 DIV 2),"
                        "c71 int as (10 MOD 2),"
                        "c72 int as (2 > 2),"
                        "c73 int as (2 >= 2),"
                        "c74 int as (2<2),"
                        "c75 int as (2<=2),"
                        "c76 int as (2=2),"
                        "c74 int as ('.01' <> '0.01'),"
                        "c75 int as (2 BETWEEN 1 AND 3),"
                        "c76 int as ( not (2 BETWEEN 1 AND 3)),"
                        "c77 int as (2 IN (0,3,5,7)),"
                        "c78 int as (1 IS TRUE),"
                        "c79 int as (1 IS NOT UNKNOWN),"
                        "c80 int as (0 IS NULL),"
                        "c81  int as ('a' like 'ae'),"
                        "c82 int as ('bar' NOT LIKE '%baz%'),"
                        "c83 int as (2 not IN (0,3,5,7)))";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER,\n"
                        "\t`c2` INTEGER GENERATED ALWAYS AS (`c1` + 1) ,\n"
                        "\t`c3` INTEGER GENERATED ALWAYS AS (`c2` + `c1`) ,\n"
                        "\t`c4` DATE GENERATED ALWAYS AS (ADDDATE('2008-01-02',INTERVAL 31 DAY)) ,\n"
                        "\t`c5` TIME GENERATED ALWAYS AS (ADDDATE('2008-01-02',31)) ,\n"
                        "\t`c6` TIMESTAMP GENERATED ALWAYS AS (CURRENT_TIMESTAMP)  NULL ,\n"
                        "\t`c7` DATE GENERATED ALWAYS AS (DATE('2003-12-31 01:02:03')) ,\n"
                        "\t`c8` TIMESTAMP GENERATED ALWAYS AS (LOCALTIME())  NULL ,\n"
                        "\t`c9` TIMESTAMP GENERATED ALWAYS AS (LOCALTIMESTAMP)  NULL ,\n"
                        "\t`c10` TIMESTAMP GENERATED ALWAYS AS (NOW())  NULL ,\n"
                        "\t`c11` DATE GENERATED ALWAYS AS (SUBDATE('2008-01-02',INTERVAL 31 DAY)) ,\n"
                        "\t`c12` TIMESTAMP (6) GENERATED ALWAYS AS (SUBTIME('2007-12-31 23:59:59.999999','1 1:1:1.000002'))  NULL ,\n"
                        "\t`c13` TIMESTAMP GENERATED ALWAYS AS (TIMESTAMP('2003-12-31'))  NULL ,\n"
                        "\t`c14` DATE GENERATED ALWAYS AS (MAKEDATE(2011,31)) ,\n"
                        "\t`c15` INTEGER GENERATED ALWAYS AS (TO_DAYS('2007-12-31 23:59:59') - TO_DAYS('2007-12-30')) ,\n"
                        "\t`c16` TIME GENERATED ALWAYS AS (MAKETIME(12,15,30)) ,\n"
                        "\t`c17` TIME GENERATED ALWAYS AS (TIME('2003-12-31 01:02:03')) ,\n"
                        "\t`c18` TIME GENERATED ALWAYS AS (TIMEDIFF('2000:01:01 00:00:00','2000:01:01 00:00:00.000001')) ,\n"
                        "\t`c19` INTEGER GENERATED ALWAYS AS (YEAR('1987-01-01')) ,\n"
                        "\t`c20` INTEGER GENERATED ALWAYS AS (ABS(2)) ,\n"
                        "\t`c21` INTEGER GENERATED ALWAYS AS (CHAR_LENGTH('aaaaaa')) ,\n"
                        "\t`c22` INTEGER GENERATED ALWAYS AS (FIELD('Bb','Aa','Bb','Cc','Dd','Ff')) ,\n"
                        "\t`c23` INTEGER GENERATED ALWAYS AS (FIND_IN_SET('b','a,b,c,d')) ,\n"
                        "\t`c24` INTEGER GENERATED ALWAYS AS (LOCATE('bar','foobarbar')) ,\n"
                        "\t`c25` INTEGER GENERATED ALWAYS AS (STRCMP('text','text2')) ,\n"
                        "\t`c26` CHAR (20) GENERATED ALWAYS AS (FORMAT(12332.123456,4)) ,\n"
                        "\t`c27` INTEGER GENERATED ALWAYS AS (POSITION('12' IN '1234')) ,\n"
                        "\t`c29` INTEGER GENERATED ALWAYS AS (DAYOFMONTH('2007-02-03')) ,\n"
                        "\t`c30` INTEGER GENERATED ALWAYS AS (DAYOFWEEK('2007-02-03')) ,\n"
                        "\t`c31` INTEGER GENERATED ALWAYS AS (DAYOFYEAR('2007-02-03')) ,\n"
                        "\t`c32` INTEGER GENERATED ALWAYS AS (HOUR('10:05:03')) ,\n"
                        "\t`c33` INTEGER GENERATED ALWAYS AS (MINUTE('2008-02-03 10:05:03')) ,\n"
                        "\t`c34` INTEGER GENERATED ALWAYS AS (MONTH('2008-02-03')) ,\n"
                        "\t`c35` INTEGER GENERATED ALWAYS AS (PERIOD_DIFF(200802,200703)) ,\n"
                        "\t`c36` INTEGER GENERATED ALWAYS AS (QUARTER('2008-04-01')) ,\n"
                        "\t`c37` INTEGER GENERATED ALWAYS AS (SECOND('10:05:03')) ,\n"
                        "\t`c38` INTEGER GENERATED ALWAYS AS (TO_DAYS(950501)) ,\n"
                        "\t`c39` CHAR (20) GENERATED ALWAYS AS (CONCAT('My','S','QL')) ,\n"
                        "\t`c40` CHAR (40) GENERATED ALWAYS AS (CONCAT_WS(',','First name','Second name','Last Name')) ,\n"
                        "\t`c41` CHAR (20) GENERATED ALWAYS AS (INSERT('Quadratic',3,4,'What')) ,\n"
                        "\t`c42` CHAR (20) GENERATED ALWAYS AS (LEFT('foobarbar',5)) ,\n"
                        "\t`c43` CHAR (20) GENERATED ALWAYS AS (LOWER('QUADRATICALLY')) ,\n"
                        "\t`c44` CHAR (20) GENERATED ALWAYS AS (LPAD('hi',4,'\?\?')) ,\n"
                        "\t`c45` CHAR (20) GENERATED ALWAYS AS (RPAD('hi',5,'?')) ,\n"
                        "\t`c46` CHAR (20) GENERATED ALWAYS AS (LTRIM('  barbar')) ,\n"
                        "\t`c47` CHAR (20) GENERATED ALWAYS AS (RTRIM('barbar   ')) ,\n"
                        "\t`c48` CHAR (20) GENERATED ALWAYS AS (REPEAT('MySQL',3)) ,\n"
                        "\t`c49` CHAR (20) GENERATED ALWAYS AS (REPLACE('www.****.com','w','Ww')) ,\n"
                        "\t`c50` CHAR (20) GENERATED ALWAYS AS (REVERSE('abc')) ,\n"
                        "\t`c51` CHAR (20) GENERATED ALWAYS AS (RIGHT('foobarbar',4)) ,\n"
                        "\t`c52` CHAR (20) GENERATED ALWAYS AS (SPACE(6)) ,\n"
                        "\t`c53` CHAR (20) GENERATED ALWAYS AS (SUBSTR('foobarbar' FROM 4)) ,\n"
                        "\t`c54` CHAR (20) GENERATED ALWAYS AS (SUBSTR('Quadratically',5)) ,\n"
                        "\t`c55` CHAR (20) GENERATED ALWAYS AS (SUBSTR('Quadratically',5,6)) ,\n"
                        "\t`c55` CHAR (20) GENERATED ALWAYS AS (SUBSTR('Sakila',-3)) ,\n"
                        "\t`c56` CHAR (20) GENERATED ALWAYS AS (SUBSTR('Sakila',-5,3)) ,\n"
                        "\t`c57` CHAR (20) GENERATED ALWAYS AS (SUBSTR('Sakila' FROM -4 FOR 2)) ,\n"
                        "\t`c58` CHAR (20) GENERATED ALWAYS AS (TRIM('  bar   ')) ,\n"
                        "\t`c59` CHAR (20) GENERATED ALWAYS AS (TRIM(LEADING 'x' FROM 'xxxbarxxx')) ,\n"
                        "\t`c60` CHAR (20) GENERATED ALWAYS AS (TRIM(BOTH 'x' FROM 'xxxbarxxx')) ,\n"
                        "\t`c61` CHAR (20) GENERATED ALWAYS AS (TRIM(TRAILING 'xyz' FROM 'barxxyz')) ,\n"
                        "\t`c62` CHAR (20) GENERATED ALWAYS AS (UPPER('aaaa')) ,\n"
                        "\t`c63` INTEGER GENERATED ALWAYS AS (SQRT(4)) ,\n"
                        "\t`c64` INTEGER GENERATED ALWAYS AS (3 + 5) ,\n"
                        "\t`c65` INTEGER GENERATED ALWAYS AS (3 - 5) ,\n"
                        "\t`c66` INTEGER GENERATED ALWAYS AS (-5) ,\n"
                        "\t`c67` INTEGER GENERATED ALWAYS AS (3 * 5) ,\n"
                        "\t`c68` INTEGER GENERATED ALWAYS AS (10 / 2) ,\n"
                        "\t`c69` INTEGER GENERATED ALWAYS AS (10 % 2) ,\n"
                        "\t`c70` INTEGER GENERATED ALWAYS AS (5 DIV 2) ,\n"
                        "\t`c71` INTEGER GENERATED ALWAYS AS (10 MOD 2) ,\n"
                        "\t`c72` INTEGER GENERATED ALWAYS AS (2 > 2) ,\n"
                        "\t`c73` INTEGER GENERATED ALWAYS AS (2 >= 2) ,\n"
                        "\t`c74` INTEGER GENERATED ALWAYS AS (2 < 2) ,\n"
                        "\t`c75` INTEGER GENERATED ALWAYS AS (2 <= 2) ,\n"
                        "\t`c76` INTEGER GENERATED ALWAYS AS (2 = 2) ,\n"
                        "\t`c74` INTEGER GENERATED ALWAYS AS ('.01' != '0.01') ,\n"
                        "\t`c75` INTEGER GENERATED ALWAYS AS (2 BETWEEN 1 AND 3) ,\n"
                        "\t`c76` INTEGER GENERATED ALWAYS AS (NOT (2 BETWEEN 1 AND 3)) ,\n"
                        "\t`c77` INTEGER GENERATED ALWAYS AS (2 IN (0,3,5,7)) ,\n"
                        "\t`c78` INTEGER GENERATED ALWAYS AS (1 IS TRUE) ,\n"
                        "\t`c79` INTEGER GENERATED ALWAYS AS (1 IS NOT UNKNOWN) ,\n"
                        "\t`c80` INTEGER GENERATED ALWAYS AS (0 IS NULL) ,\n"
                        "\t`c81` INTEGER GENERATED ALWAYS AS ('a' LIKE 'ae') ,\n"
                        "\t`c82` INTEGER GENERATED ALWAYS AS ('bar' NOT LIKE '%baz%') ,\n"
                        "\t`c83` INTEGER GENERATED ALWAYS AS (2 NOT IN (0,3,5,7)) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_2) {
  std::string source = "create table jsonfunc (\n"
   "id int primary key,\n"
    "json_info json,\n"
                "c1 json generated always as (JSON_ARRAY(1, \"abc\", NULL, TRUE)),\n"
                "c2 json generated always as (JSON_OBJECT('id', 87, 'name', 'carrot')),\n"
                "c3 json generated always as (JSON_QUOTE('null')),\n"
                "c4 json generated always as (JSON_UNQUOTE('null')),\n"
                "c5 json generated always as (JSON_EXTRACT('[10, 20, [30, 40]]', '$[1]')),\n"
                "c6 json generated always as (JSON_KEYS('{\"a\": 1, \"b\": {\"c\": 30}}')),\n"
                "c7 json generated always as (JSON_SEARCH('[\"abc\", [{\"k\": \"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', 'one', 'abc')),\n"
                "c8 json generated always as (JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]', 1)),\n"
                "c9 json generated always as (JSON_MERGE_PRESERVE('[1, 2]', '[true, false]')),\n"
                "c10 json generated always as (JSON_REMOVE('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]')),\n"
                "c11 json generated always as (JSON_DEPTH('true')),\n"
                "c12 json generated always as (JSON_LENGTH('{\"a\": 1, \"b\": {\"c\": 30}}', '$.b')),\n"
                "c13 json generated always as (JSON_REPLACE('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')),\n"
                "c14 json generated always as (json_info -> '$.id'),\n"
                "c15 json generated always as (json_info ->> '$.id'),\n"
                "c17 json generated always as (JSON_MERGE_PATCH('[1, 2]', '[true, false]')),\n"
                "c16 json generated always as (JSON_SET('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')));";
  std::string expect = "CREATE TABLE `jsonfunc`(\n"
   "\t`id` INTEGER NOT NULL ,\n"
    "\t`json_info` JSON,\n"
                "\t`c1` JSON GENERATED ALWAYS AS (JSON_ARRAY(1,\"abc\",NULL,TRUE)) ,\n"
                "\t`c2` JSON GENERATED ALWAYS AS (JSON_OBJECT('id',87,'name','carrot')) ,\n"
                "\t`c3` JSON GENERATED ALWAYS AS (JSON_QUOTE('null')) ,\n"
                "\t`c4` JSON GENERATED ALWAYS AS (JSON_UNQUOTE('null')) ,\n"
                "\t`c5` JSON GENERATED ALWAYS AS (JSON_EXTRACT('[10, 20, [30, 40]]','$[1]')) ,\n"
                "\t`c6` JSON GENERATED ALWAYS AS (JSON_KEYS('{\"a\": 1, \"b\": {\"c\": 30}}')) ,\n"
                "\t`c7` JSON GENERATED ALWAYS AS (JSON_SEARCH('[\"abc\", [{\"k\": \"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]','one','abc')) ,\n"
                "\t`c8` JSON GENERATED ALWAYS AS (JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]','$[1]',1)) ,\n"
                "\t`c9` JSON GENERATED ALWAYS AS (JSON_MERGE_PRESERVE('[1, 2]','[true, false]')) ,\n"
                "\t`c10` JSON GENERATED ALWAYS AS (JSON_REMOVE('[\"a\", [\"b\", \"c\"], \"d\"]','$[1]')) ,\n"
                "\t`c11` JSON GENERATED ALWAYS AS (JSON_DEPTH('true')) ,\n"
                "\t`c12` JSON GENERATED ALWAYS AS (JSON_LENGTH('{\"a\": 1, \"b\": {\"c\": 30}}','$.b')) ,\n"
                "\t`c13` JSON GENERATED ALWAYS AS (JSON_REPLACE('{ \"a\": 1, \"b\": [2, 3]}','$.a',10,'$.c','[true, false]')) ,\n"
                "\t`c14` JSON GENERATED ALWAYS AS (`json_info` -> '$.id') ,\n"
                "\t`c15` JSON GENERATED ALWAYS AS (`json_info` ->> '$.id') ,\n"
                "\t`c17` JSON GENERATED ALWAYS AS (JSON_MERGE_PATCH('[1, 2]','[true, false]')) ,\n"
                "\t`c16` JSON GENERATED ALWAYS AS (JSON_SET('{ \"a\": 1, \"b\": [2, 3]}','$.a',10,'$.c','[true, false]')) ,\n"
                "\tPRIMARY KEY (`id`)\n"
                 ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_3) {
  std::string source = "create table jsonfunc (\n"
   "c1 json generated always as (JSON_ARRAY(1, \"abc\", NULL, TRUE)),\n"
                "c2 json generated always as (JSON_OBJECT('id', 87, 'name', 'carrot')),\n"
                "c3 json generated always as (JSON_QUOTE('null')),\n"
                "c4 json generated always as (JSON_UNQUOTE('null')),\n"
                "c5 json generated always as (JSON_EXTRACT('[10, 20, [30, 40]]', '$[1]')),\n"
                "c6 json generated always as (JSON_KEYS('{\"a\": 1, \"b\": {\"c\": 30}}')),\n"
                "c7 json generated always as (JSON_SEARCH('[\"abc\", [{\"k\": \"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', 'one', 'abc')),\n"
                "c8 json generated always as (JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]', 1)),\n"
                "c9 json generated always as (JSON_MERGE('[1, 2]', '[true, false]')),\n"
                "c10 json generated always as (JSON_REMOVE('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]')),\n"
                "c11 json generated always as (JSON_DEPTH('true')),\n"
                "c12 json generated always as (JSON_LENGTH('{\"a\": 1, \"b\": {\"c\": 30}}', '$.b')),\n"
                "c13 json generated always as (JSON_REPLACE('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')),\n"
                "c14 json generated always as (json_info -> '$.id'),\n"
                "c15 json generated always as (json_info ->> '$.id'),\n"
                "c16 json generated always as (JSON_SET('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')));";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_4) {
  std::string source = "create table t1(c1 int, c2 int as (c1 + 1), c3 int as (c2 + c1))";
  std::string expect = "CREATE TABLE `t1`(\n"
                        "\t`c1` INTEGER,\n"
                        "\t`c2` INTEGER GENERATED ALWAYS AS (`c1` + 1) ,\n"
                        "\t`c3` INTEGER GENERATED ALWAYS AS (`c2` + `c1`) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_5) {
  std::string source = "create table t(c1 int, c2 int generated always as (c1 like '/_hello' escape '/'));";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`c1` INTEGER,\n"
                        "\t`c2` INTEGER GENERATED ALWAYS AS (`c1` LIKE '/_hello' escape '/') \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_6) {
  std::string source = "create table connector_test.generate_column_stored_test\n"
                        "(\n"
                        "    id bigint not null,\n"
                        "    c1 varchar(512) generated always as (case when id < 10 then 'small' else 'big' end) stored,\n"
                        "    c2 varchar(512),\n"
                        "    c3 varchar(512),\n"
                        "    primary key (id)\n"
                        ") CHARSET = 'utf8';";
  std::string expect = "CREATE TABLE `generate_column_stored_test`(\n"
                        "\t`id` BIGINT NOT NULL ,\n"
                        "\t`c1` VARCHAR (512) GENERATED ALWAYS AS (CASE  WHEN `id` < 10 THEN 'small' ELSE 'big' END) STORED ,\n"
                        "\t`c2` VARCHAR (512),\n"
                        "\t`c3` VARCHAR (512),\n"
                        "\tPRIMARY KEY (`id`)\n"
                        ") CHARACTER SET = 'utf8'";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_TABLE, CREATE_TABLE_GENERATED_COLUMN_7) {
  std::string source = "create table t (c1 int,c2 int generated always as (mod(c1,2)));";
  std::string expect = "CREATE TABLE `t`(\n"
                        "\t`c1` INTEGER,\n"
                        "\t`c2` INTEGER GENERATED ALWAYS AS (MOD(`c1`,2)) \n"
                        ")";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(OTHER, OTHER_1) {
  std::string source = "\n;;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(OTHER, OTHER_2) {
  std::string source = "\n;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_1) {
  std::string source = "create table jsonfunc (\n"
                "c1 json generated always as (JSON_ARRAY(1, \"abc\", NULL, TRUE)),\n"
                "c2 json generated always as (JSON_OBJECT('id', 87, 'name', 'carrot')),\n"
                "c3 json generated always as (JSON_QUOTE('null')),\n"
                "c4 json generated always as (JSON_UNQUOTE('null')),\n"
                "c5 json generated always as (JSON_EXTRACT('[10, 20, [30, 40]]', '$[1]')),\n"
                "c6 json generated always as (JSON_KEYS('{\"a\": 1, \"b\": {\"c\": 30}}')),\n"
                "c7 json generated always as (JSON_SEARCH('[\"abc\", [{\"k\": \"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]', 'one', 'abc')),\n"
                "c8 json generated always as (JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]', 1)),\n"
                "c9 json generated always as (JSON_MERGE('[1, 2]', '[true, false]')),\n"
                "c10 json generated always as (JSON_REMOVE('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]')),\n"
                "c11 json generated always as (JSON_DEPTH('true')),\n"
                "c12 json generated always as (JSON_LENGTH('{\"a\": 1, \"b\": {\"c\": 30}}', '$.b')),\n"
                "c13 json generated always as (JSON_REPLACE('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')),\n"
                "c14 json generated always as (json_info -> '$.id'),\n"
                "c15 json generated always as (json_info ->> '$.id'),\n"
                "c16 json generated always as (JSON_SET('{ \"a\": 1, \"b\": [2, 3]}', '$.a', 10, '$.c', '[true, false]')));";
  std::string expect = "CREATE TABLE `jsonfunc`(\n"
                "\t`c1` JSON GENERATED ALWAYS AS (JSON_ARRAY(1,\"abc\",NULL,TRUE)) ,\n"
                "\t`c2` JSON GENERATED ALWAYS AS (JSON_OBJECT('id',87,'name','carrot')) ,\n"
                "\t`c3` JSON GENERATED ALWAYS AS (JSON_QUOTE('null')) ,\n"
                "\t`c4` JSON GENERATED ALWAYS AS (JSON_UNQUOTE('null')) ,\n"
                "\t`c5` JSON GENERATED ALWAYS AS (JSON_EXTRACT('[10, 20, [30, 40]]','$[1]')) ,\n"
                "\t`c6` JSON GENERATED ALWAYS AS (JSON_KEYS('{\"a\": 1, \"b\": {\"c\": 30}}')) ,\n"
                "\t`c7` JSON GENERATED ALWAYS AS (JSON_SEARCH('[\"abc\", [{\"k\": \"10\"}, \"def\"], {\"x\":\"abc\"}, {\"y\":\"bcd\"}]','one','abc')) ,\n"
                "\t`c8` JSON GENERATED ALWAYS AS (JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]','$[1]',1)) ,\n"
                "\t`c9` JSON GENERATED ALWAYS AS (JSON_MERGE('[1, 2]','[true, false]')) ,\n"
                "\t`c10` JSON GENERATED ALWAYS AS (JSON_REMOVE('[\"a\", [\"b\", \"c\"], \"d\"]','$[1]')) ,\n"
                "\t`c11` JSON GENERATED ALWAYS AS (JSON_DEPTH('true')) ,\n"
                "\t`c12` JSON GENERATED ALWAYS AS (JSON_LENGTH('{\"a\": 1, \"b\": {\"c\": 30}}','$.b')) ,\n"
                "\t`c13` JSON GENERATED ALWAYS AS (JSON_REPLACE('{ \"a\": 1, \"b\": [2, 3]}','$.a',10,'$.c','[true, false]')) ,\n"
                "\t`c14` JSON GENERATED ALWAYS AS (`json_info` -> '$.id') ,\n"
                "\t`c15` JSON GENERATED ALWAYS AS (`json_info` ->> '$.id') ,\n"
                "\t`c16` JSON GENERATED ALWAYS AS (JSON_SET('{ \"a\": 1, \"b\": [2, 3]}','$.a',10,'$.c','[true, false]')) \n"
                ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 7, 13);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_2) {
  std::string source = "create table test_on_update3 (\n"
                     "  dt1 datetime ,\n"
                     "  dt2 datetime not null on update current_timestamp,\n"
                     "  id int ,\n"
                     "primary key(id));";
  std::string expect = "CREATE TABLE `test_on_update3`(\n"
                     "\t`dt1` DATETIME,\n"
                     "\t`dt2` DATETIME NOT NULL ,\n"
                     "\t`id` INTEGER NOT NULL ,\n"
                     "\tPRIMARY KEY (`id`)\n" 
                     ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_3) {
  std::string source = "create table test_on_update3 (\n"
                     "  dt1 timestamp(1) ,\n"
                     "id int ,\n"
                     "primary key(id));";
  std::string expect = "CREATE TABLE `test_on_update3`(\n"
                     "\t`dt1` TIMESTAMP NULL ,\n"
                     "\t`id` INTEGER NOT NULL ,\n"
                     "\tPRIMARY KEY (`id`)\n"
                     ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_4) {
  std::string source = "alter table t1 add column c1 timestamp(3)";
  std::string expect = "ALTER TABLE `t1`\n\t\tADD COLUMN `c1` TIMESTAMP NULL \n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_5) {
  std::string source = "alter table t1 add column c1 time(3)";
  std::string expect = "ALTER TABLE `t1`\n\t\tADD COLUMN `c1` TIME\n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_6) {
  std::string source = "create table t1(c1 time(3), c2 int, primary key(c2))";
  std::string expect = "CREATE TABLE `t1`(\n\t`c1` TIME,\n\t`c2` INTEGER NOT NULL ,\n\tPRIMARY KEY (`c2`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_7) {
  std::string source = "alter table t1 add column c1 datetime(6)";
  std::string expect = "ALTER TABLE `t1`\n\t\tADD COLUMN `c1` DATETIME\n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_8) {
  std::string source = "create table t1(c1 datetime(6), c2 int, primary key(c2))";
  std::string expect = "CREATE TABLE `t1`(\n\t`c1` DATETIME,\n\t`c2` INTEGER NOT NULL ,\n\tPRIMARY KEY (`c2`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_9) {
  std::string source = "create table vir_test (\n" 
                "  id bigint(20) not null auto_increment,\n"
                "  create_at timestamp not null default current_timestamp,\n"
                "  create_at_dayofweek tinyint(4) generated always as (dayofweek(create_at)) virtual,\n"
                "  primary key (id),\n"
                "  key idx_create_at_dayofweek (create_at_dayofweek)\n"
                ") engine=innodb default charset=utf8mb4;";
  std::string expect = "CREATE TABLE `vir_test`(\n"
                "\t`id` BIGINT (20) NOT NULL  AUTO_INCREMENT ,\n"
                "\t`create_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL ,\n"
                "\t`create_at_dayofweek` TINYINT (4),\n"
                "\tPRIMARY KEY (`id`),\n"
                "\tINDEX `idx_create_at_dayofweek`(`create_at_dayofweek`) \n"
                ") CHARACTER SET = utf8mb4";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 5, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_10) {
  std::string source = "create table ttt(c1 GEOMETRY, c2 POINT SRID 4326, c3 LINESTRING, "
                        "c4 POLYGON SRID 1, c5 MULTIPOINT SRID 2, c6 MULTILINESTRING, c7 MULTIPOLYGON, c8 GEOMETRYCOLLECTION);";
  std::string expect = "CREATE TABLE `ttt`(\n"
                        "\t`c1` GEOMETRY,\n"
                        "\t`c2` POINT SRID 4326 ,\n"
                        "\t`c3` LINESTRING,\n"
                        "\t`c4` POLYGON SRID 1 ,\n"
                        "\t`c5` MULTIPOINT SRID 2 ,\n"
                        "\t`c6` MULTILINESTRING,\n"
                        "\t`c7` MULTIPOLYGON,\n"
                        "\t`c8` GEOMETRYCOLLECTION\n"
                        ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(8, 0, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_11) {
  std::string source = "create table ttt(c1 GEOMETRY, c2 POINT SRID 4326, c3 LINESTRING, "
                        "c4 POLYGON SRID 1, c5 MULTIPOINT SRID 2, c6 MULTILINESTRING, c7 MULTIPOLYGON, c8 GEOMETRYCOLLECTION);";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 7, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_12) {
  std::string source = "alter table tt add column c1 GEOMETRY, add column c2 POINT SRID 4326;";
  std::string expect = "ALTER TABLE `tt`\n"
                        "\t\tADD COLUMN `c1` GEOMETRY, \n"
                        "\t\tADD COLUMN `c2` POINT SRID 4326 \n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(8, 0, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_13) {
  std::string source = "alter table tt add column c1 GEOMETRY, add column c2 POINT SRID 4326;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 6, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_14) {
  std::string source = "CREATE SPATIAL INDEX gidx ON t(col1);";
  std::string expect = "\tCREATE SPATIAL INDEX `gidx` ON `t`(`col1`) ";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 7, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, VERSION_15) {
  std::string source = "CREATE SPATIAL INDEX gidx ON t(col1);";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->SetDbVersion(5, 6, 0);
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), -1);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, SCHEMA_PREFIX) {
  std::string source = "ALTER TABLE sc.wip_history RENAME INDEX station_code_2 TO idx_station_code";
  std::string expect = "ALTER TABLE `sc`.`wip_history`\n"
                        "\tRENAME INDEX `station_code_2` TO `idx_station_code`\n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_schema_prefix = true;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, FOREIGN_KEY_1) {
  std::string source = "alter table tt2 add constraint fk1 foreign key(c1, c2) references tt1(c1, c2)";
  std::string expect = "ALTER TABLE `tt2`\n"
                        "\t\tADD CONSTRAINT `fk1` FOREIGN KEY (`c1`,`c2`) REFERENCES `tt1`(`c1`,`c2`)  \n";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_foreign_key_filter = false;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, FOREIGN_KEY_2) {
  std::string source = "create table t2_9090(c1 int primary key, t1_c1 int, constraint fk_1 foreign key(t1_c1) references t1_9090(c1))";
  std::string expect = "CREATE TABLE `t2_9090`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\t`t1_c1` INTEGER,\n"
                        "\tCONSTRAINT `fk_1` FOREIGN KEY (`t1_c1`) REFERENCES `t1_9090`(`c1`)  ,\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_foreign_key_filter = false;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(PARSER_WITH_CONTEXT, FOREIGN_KEY_3) {
  std::string source = "create table t2_9090(c1 int primary key, t1_c1 int, constraint fk_1 foreign key(t1_c1) references t1_9090(c1))";
  std::string expect = "CREATE TABLE `t2_9090`(\n"
                        "\t`c1` INTEGER NOT NULL ,\n"
                        "\t`t1_c1` INTEGER,\n"
                        "\tPRIMARY KEY (`c1`)\n"
                        ")";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, DEFAULT_CHARSET) {
  // 1
  std::string source = "create table t1(c1 int not null primary key, c2 char(32));";
  std::string expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") CHARACTER SET = utf8, COLLATE = utf8mb4_0900_ai_ci";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "utf8";
  parse_context->default_collection = "utf8mb4_0900_ai_ci";
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());

  // 2
  source = "create table t1(c1 int not null primary key, c2 char(32)) CHARACTER SET = utf16;";
  expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") CHARACTER SET = utf16, COLLATE = utf8mb4_0900_ai_ci";
  parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "utf8";
  parse_context->default_collection = "utf8mb4_0900_ai_ci";
  dest.clear();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
  // 3
  source = "create table t1(c1 int not null primary key, c2 char(32)) CHARACTER SET = utf16, COLLATE = utf8mb16_0900_ai_ci;";
  expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") CHARACTER SET = utf16, COLLATE = utf8mb16_0900_ai_ci";
  parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "utf8";
  parse_context->default_collection = "utf8mb4_0900_ai_ci";
  dest.clear();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());

  // 4
  source = "create table t1(c1 int not null primary key, c2 char(32)) COLLATE = utf8mb16_0900_ai_ci;";
  expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") COLLATE = utf8mb16_0900_ai_ci, CHARACTER SET = utf8";
  parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "utf8";
  parse_context->default_collection = "utf8mb4_0900_ai_ci";
  dest.clear();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());

  // 5
  source = "create table t1(c1 int not null primary key, c2 char(32));";
  expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") CHARACTER SET = utf8";
  parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "utf8";
  parse_context->default_collection = "";
  dest.clear();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());

  // 6
  source = "create table t1(c1 int not null primary key, c2 char(32));";
  expect = "CREATE TABLE `t1`(\n"
                "\t`c1` INTEGER NOT NULL ,\n"
                "\t`c2` CHAR (32),\n"
                "\tPRIMARY KEY (`c1`)\n"
                ") COLLATE = utf8mb4_0900_ai_ci";
  parse_context = std::make_shared<ParseContext>(source, "", false);
  parse_context->default_charset = "";
  parse_context->default_collection = "utf8mb4_0900_ai_ci";
  dest.clear();
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

// new case
TEST(CREATE_TABLE, CREATE_TABLE_FAILED_1) {
  std::string source = "create table t as select a, b from s;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);
  std::cout << err_msg << std::endl;

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_FAILED_1) {
  std::string source = "alter table t drop tablegroup;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);
  std::cout << err_msg << std::endl;

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(ALTER_TABLE, ALTER_TABLE_43) {
  std::string source = "alter table t drop primary key";
  std::string expect = "ALTER TABLE `t`\n"
                       "\tDROP PRIMARY KEY \n";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(CREATE_VIEW, CREATE_VIEW_1) {
  std::string source = "create view v as select a from t;";
  std::string expect = "";
  std::string dest;
  std::string err_msg;
  ASSERT_EQ(etransfer::tool::ConvertTool::Parse(source, "", false, dest, err_msg), -1);
  std::cout << err_msg << std::endl;

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_1) {
  std::string source = "create table t(c1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE `test`.`t`(\n\t`c1` INTEGER NOT NULL ,"
                       "\n\t`t1_c1` INTEGER,\n\tPRIMARY KEY (`c1`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_schema_prefix = true;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_2) {
  std::string source = "create table db.t(c1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE `db`.`t`(\n\t`c1` INTEGER NOT NULL ,"
                       "\n\t`t1_c1` INTEGER,\n\tPRIMARY KEY (`c1`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_schema_prefix = true;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_3) {
  std::string source = "create table db.t(c1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE 't'(\n\t'c1' INTEGER NOT NULL ,"
                       "\n\t't1_c1' INTEGER,\n\tPRIMARY KEY ('c1')\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->escape_char = "\'";
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_4) {
  std::string source = "create table db.t(c1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE t(\n\tc1 INTEGER NOT NULL ,"
                       "\n\tt1_c1 INTEGER,\n\tPRIMARY KEY (c1)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_escape_string = false;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_5) {
  std::string source = "create table db.T(C1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE `T`(\n\t`C1` INTEGER NOT NULL ,"
                       "\n\t`t1_c1` INTEGER,\n\tPRIMARY KEY (`C1`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_origin_identifier = true;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_6) {
  std::string source = "create table db.T(C1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE `t`(\n\t`C1` INTEGER NOT NULL ,"
                       "\n\t`t1_c1` INTEGER,\n\tPRIMARY KEY (`C1`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  builder_context->use_origin_identifier = false;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);

  EXPECT_STREQ(expect.c_str(), dest.c_str());
}

TEST(PARSER_WITH_CONTEXT, CONTEXT_7) {
  std::string source = "create table T(C1 int primary key, t1_c1 int)";
  std::string expect = "CREATE TABLE `t`(\n\t`C1` INTEGER NOT NULL ,"
                       "\n\t`t1_c1` INTEGER,\n\tPRIMARY KEY (`C1`)\n)";
  std::string dest;
  std::string err_msg;
  std::shared_ptr<ParseContext> parse_context = std::make_shared<ParseContext>(source, "test", false);
  std::shared_ptr<BuildContext> builder_context = std::make_shared<BuildContext>();
  parse_context->is_case_sensitive = true;
  ASSERT_EQ(etransfer::tool::ConvertTool::ParseWithContext(source, parse_context, builder_context, dest, err_msg), 0);
  std::cout << err_msg << std::endl;
  EXPECT_STREQ(expect.c_str(), dest.c_str());
}