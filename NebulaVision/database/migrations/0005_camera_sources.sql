USE nebulavision_dev;
ALTER TABLE cameras ADD COLUMN source_type VARCHAR(16) NOT NULL DEFAULT 'relay';
ALTER TABLE cameras ADD COLUMN source_key VARCHAR(64) NULL;
ALTER TABLE cameras ADD UNIQUE KEY uq_camera_source_key(source_key);
CREATE TABLE live_event_receipts (
 request_key VARCHAR(36) PRIMARY KEY,
 event_id BIGINT UNSIGNED NOT NULL,
 camera_id BIGINT UNSIGNED NOT NULL,
 actor_id BIGINT UNSIGNED NOT NULL,
 FOREIGN KEY(event_id) REFERENCES events(id),
 FOREIGN KEY(camera_id) REFERENCES cameras(id),
 FOREIGN KEY(actor_id) REFERENCES users(id)
) ENGINE=InnoDB;
INSERT INTO schema_migrations(version,description) VALUES ('0005','区分本机、直连RTSP及媒体中继设备');
