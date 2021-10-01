use transtest;

CREATE TABLE `dataworkstest` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `k` varchar(100) NOT NULL,
  `v` varchar(100) NOT NULL,
  PRIMARY KEY (`id`)
) DEFAULT CHARSET = utf8mb4;

CREATE TABLE `dataworkstest2` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `k` varchar(100) NOT NULL,
  `v` varchar(100) NOT NULL,
  `status` tinyint(10) NOT NULL,
  `create_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) DEFAULT CHARSET = utf8mb4;
