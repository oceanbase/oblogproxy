package com.oceanbase.clogproxy.demo;

import com.google.common.base.Joiner;
import com.google.common.collect.Lists;
import com.oceanbase.clogproxy.client.ErrorCode;
import com.oceanbase.clogproxy.client.LogProxyClient;
import com.oceanbase.clogproxy.client.LogProxyClientException;
import com.oceanbase.clogproxy.client.RecordListener;
import com.oceanbase.clogproxy.client.config.ClientConf;
import com.oceanbase.clogproxy.client.config.ObReaderConfig;
import com.oceanbase.oms.record.Field;
import com.oceanbase.oms.record.RecordType;
import com.oceanbase.oms.record.Struct;
import com.oceanbase.oms.record.oms.OmsIndexSchema;
import com.oceanbase.oms.record.oms.OmsRecord;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.DefaultParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * <p>
 * <p> Date: 2020/6/8 </p>
 *
 * @author Fankux(yuqi.fy)
 */
public class Demo {
    private static final Logger logger = LoggerFactory.getLogger(Demo.class);

    public static void main(String[] args) {
        Options options = new Options();
        options.addOption("i", "instanceId", true, "cluster Id");
        options.addOption("l", "instanceUrl", true, "cluster Url");
        options.addOption("h", "host", true, "host address");
        options.addOption("P", "port", true, "host port");
        options.addOption("u", "username", true, "username to connect");
        options.addOption("p", "password", true, "password to connect");
        options.addOption("c", "checkpoint", true, "checkpoint to start(UNIX timestamp)");
        options.addOption("t", "tableWhiteList", true, "table whilte list to filter");
        options.addOption("e", "properties", true, "extra K-V properties");

        String address = "";
        int port = 0;
        ObReaderConfig config = new ObReaderConfig();

        CommandLineParser parser = new DefaultParser();
        try {
            CommandLine cmd = parser.parse(options, args);
            for (Option opt : cmd.getOptions()) {
                switch (opt.getOpt()) {
                    case "i":
                        config.setInstanceId(opt.getValue());
                        break;
                    case "l":
                        config.setInstanceUrl(opt.getValue());
                        break;
                    case "h":
                        address = opt.getValue();
                        break;
                    case "P":
                        port = Integer.parseInt(opt.getValue());
                        break;
                    case "u":
                        config.setUsername(opt.getValue());
                        break;
                    case "p":
                        config.setPassword(opt.getValue());
                        break;
                    case "c":
                        config.setStartTimestamp(Long.parseLong(opt.getValue()));
                        break;
                    case "t":
                        config.setTableWhiteList(opt.getValue());
                        break;
                    case "e":
//                        config.setExtraConfigs();
                        break;
                    default:
                        for (Option option : options.getOptions()) {
                            System.out.printf("-%s,--%s\t\t\t%s%n", option.getOpt(), option.getLongOpt(), option.getDescription());
                        }
                        return;
                }
            }
        } catch (ParseException e) {
            logger.error("failed to parse command line: ", e);
            return;
        }
        if (StringUtils.isEmpty(address) || port == 0) {
            logger.error("failed to initiate caused by empty address or not port specified");
            return;
        }

        Joiner joiner = Joiner.on(',');

        logger.info("conf: {}", config.toString());
        ClientConf.USER_DEFINED_CLIENTID = "LogProxyClientDemo";
        ClientConf.IGNORE_UNKNOWN_RECORD_TYPE = true;
        LogProxyClient logProxyClient = new LogProxyClient(address, port, config);
        logProxyClient.addListener(new RecordListener() {
            @Override
            public void notify(OmsRecord record) {
                if (record.getRecordType() == RecordType.HEARTBEAT) {
                    logger.info("HEARTBEAT: {}:{}:{}", record.meta().getDbTypeRaw(), record.meta().getTimestamp(), record.meta().getCheckpoint());
                    return;
                }
                if (record.getRecordType() == RecordType.INSERT) {
                    List<String> colums = Optional.ofNullable(record.getPostStruct().fields()).orElse(Lists.newArrayList()).
                        stream().map(field -> '`' + field.name() + '`').collect(Collectors.toList());
                    List<Object> values = Optional.ofNullable(record.getPostStruct().values()).orElse(Lists.newArrayList()).
                        stream().map(o -> '\'' + o.toString() + '\'').collect(Collectors.toList());
                    assert (colums.size() != values.size());

                    String insertSql = String.format("INSERT INTO `%s`.`%s`(%s) VALUES (%s)",
                        record.meta().getDatabase(), record.meta().getTable(), joiner.join(colums), joiner.join(values));
                    logger.info(insertSql);
                    return;
                }

                if (record.getRecordType() == RecordType.DELETE) {
                    OmsIndexSchema uniqueSchema = record.getPostStruct().schema().chosenUidIndex();
                    if (uniqueSchema == null) {
                        throw new LogProxyClientException(ErrorCode.E_PARSE, "None Unique Key record: " + record.toString());
                    }

                    List<String> keyNames = uniqueSchema.getIndexFields().stream().map(Field::name).collect(Collectors.toList());
                    List<String> keyValues = uniqueSchema.getIndexFields().stream().map(f ->
                        record.getPostStruct().get(f).toString()).collect(Collectors.toList());
                    assert (keyNames.size() != keyValues.size());
                    StringBuilder deleteSql = new StringBuilder(String.format("DELETE FROM `%s`.`%s` WHERE ",
                        record.meta().getDatabase(), record.meta().getTable()));
                    for (int i = 0; i < keyNames.size(); ++i) {
                        deleteSql.append("`").append(keyNames.get(i)).append("`='").append(keyValues.get(i)).append('\'');
                        if (i != keyNames.size() - 1) {
                            deleteSql.append(" AND ");
                        }
                    }
                    logger.info(deleteSql.toString());
                }

                if (record.getRecordType() == RecordType.UPDATE) {
                    StringBuilder sb = new StringBuilder(String.format("UPDATE `%s`.`%s` SET ", record.meta().getDatabase(),
                        record.meta().getTable()));
                    List<Field> fields = record.getPostStruct().fields();
                    for (int i = 1; i < fields.size(); i += 1) {
                        sb.append("`").append(fields.get(i).name()).append("`='").
                            append(record.getPostStruct().get(fields.get(i))).append('\'');
                        if (i != fields.size() - 1) {
                            sb.append(", ");
                        }
                    }

                    sb.append(" WHERE ");

                    OmsIndexSchema uniqueSchema = record.getPostStruct().schema().chosenUidIndex();
                    if (uniqueSchema == null) {
                        throw new LogProxyClientException(ErrorCode.E_PARSE, "None Unique Key record: " + record.toString());
                    }
                    List<String> keyNames = uniqueSchema.getIndexFields().stream().map(Field::name).collect(Collectors.toList());
                    List<String> keyValues = uniqueSchema.getIndexFields().stream().map(f ->
                        record.getPostStruct().get(f).toString()).collect(Collectors.toList());
                    assert (keyNames.size() != keyValues.size());
                    StringBuilder whereSql = new StringBuilder();
                    for (int i = 0; i < keyNames.size(); ++i) {
                        whereSql.append("`").append(keyNames.get(i)).append("`").append("=").append("'").append(keyValues.get(i)).append("'");
                        if (i != keyNames.size() - 1) {
                            whereSql.append(" AND ");
                        }
                        logger.info(whereSql.toString());
                    }
                    sb.append(whereSql);

                    logger.info(sb.toString());
                }

                if (record.getRecordType() == RecordType.DDL) {
                    logger.info(record.toString());
                }
            }

            @Override
            public void onException(LogProxyClientException e) {
                logger.error("LogProxy Client exception occured: ", e);
                if (e.needStop()) {
                    logProxyClient.stop();
                }
            }
        });

        logProxyClient.addStatusListener(status -> logger.info("Runtime Status: {}", status.toString().replace("\n", ", ")));

        logProxyClient.start();
        logProxyClient.join();
    }

    static String getFieldValue(Struct struct, Field field) {
        return struct.get(field).toString();
    }
}
