"""管理员在本机更新技术文档后运行：python index_documents.py 外部资源目录。"""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
resource = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(resource / "rag-python"))
from retrieval import build_index
print("Indexed chunks:", build_index(resource, Path(__file__).resolve().parents[1] / "NebulaVision/docs"))
