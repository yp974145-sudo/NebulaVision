USE nebulavision_dev;
CREATE TABLE IF NOT EXISTS user_camera_permissions (
 user_id BIGINT UNSIGNED NOT NULL,
 camera_id BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(user_id,camera_id),
 FOREIGN KEY(user_id) REFERENCES users(id),
 FOREIGN KEY(camera_id) REFERENCES cameras(id)
) ENGINE=InnoDB;
INSERT IGNORE INTO schema_migrations(version,description) VALUES ('0003','服务端摄像头资源权限');
