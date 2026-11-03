"""使用本机模型与Qdrant持久化副本验证来源、过滤及过期证据。"""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import shutil
from qdrant_client import QdrantClient, models
import retrieval

resource, temporary, documents = map(Path, sys.argv[1:4])
model = retrieval.open_model(resource)
retrieval.open_model = lambda _: model
test = temporary / "retrieval-test"
test.mkdir(exist_ok=True)
shutil.copytree(resource / "document-index", test / "document-index", dirs_exist_ok=True)
cases = [("登录后返回按钮没有反应怎么办", "登录与返回修复说明.md"),
         ("Redis断开为什么不能直接认为摄像头离线", "阶段14_Redis状态与面试问答.md"),
         ("录像文件丢失时怎样处理", "阶段13_录像闭环与面试问答.md")]
for question, source in cases:
    report = retrieval.search(test, documents, question)
    assert source in report, (question, report)
    assert "SHA256" in report
    print("citation match:", source)
client = QdrantClient(path=str(test / "document-index"))
vector = next(model.embed([cases[0][0]])).tolist()
client.upsert(retrieval.COLLECTION, points=[models.PointStruct(id=9999, vector=vector,
    payload={"visibility": "admin", "source": "private.md", "text": "private-only-marker"})])
client.close()
assert "private-only-marker" not in retrieval.search(test, documents, cases[0][0])
copy = test / "documents"
copy.mkdir(exist_ok=True)
for name in retrieval.DOCUMENTS:
    shutil.copyfile(documents / name, copy / name)
(copy / cases[0][1]).write_text("changed after indexing", encoding="utf-8")
assert cases[0][1] not in retrieval.search(test, copy, cases[0][0])
print("PASS: three source matches, public payload filter, changed source rejected")
