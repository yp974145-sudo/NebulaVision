-- NebulaVision 新开发库；不修改、不复制 smart_monitor。
CREATE DATABASE IF NOT EXISTS nebulavision_dev CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE nebulavision_dev;
CREATE TABLE IF NOT EXISTS schema_migrations (
  version VARCHAR(32) PRIMARY KEY,
  description VARCHAR(255) NOT NULL,
  applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;
INSERT IGNORE INTO schema_migrations(version,description) VALUES ('0001','独立开发库初始化；业务表待接口设计后创建');
SELECT DATABASE() AS active_database;
SELECT version,description FROM schema_migrations;
