USE nebulavision_dev;
-- 仅合成测试数据，无可登录账号、无真实摄像头和录像。
INSERT INTO cameras(camera_key,name,status,enabled)
SELECT 'demo-camera-01','演示通道（未接入）','offline',FALSE
WHERE NOT EXISTS (SELECT 1 FROM cameras WHERE camera_key='demo-camera-01');
INSERT INTO events(event_key,camera_id,occurred_at,event_type,description,status)
SELECT 'synthetic-event-001',id,'2026-11-01 00:00:00.000','synthetic','合成测试事件，仅用于查询验证，不代表模型检测结果','test'
FROM cameras WHERE camera_key='demo-camera-01'
AND NOT EXISTS (SELECT 1 FROM events WHERE event_key='synthetic-event-001');
