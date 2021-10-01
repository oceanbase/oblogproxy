#!/usr/bin/env bash

mkdir -p bin && mvn clean package -Dmaven.test.skip=true && cp server/target/logproxy-jar-with-dependencies.jar ./bin/logproxy.jar && cp client/target/logproxy-client.jar ./bin/logproxy-client.jar && cp ./server/script/* ./bin && cp ./server/script/run.sh .
