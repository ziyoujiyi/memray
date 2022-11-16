"""Tools for processing and filtering stack frames."""
import re
from typing import Tuple

Symbol = str
File = str
Lineno = int
StackFrame = Tuple[Symbol, File, Lineno]

RE_CPYTHON_PATHS = re.compile(r"(Include|Objects|Modules|Python|cpython).*\.[c|h]$")

SYMBOL_IGNORELIST = {
    "PyObject_Call",
    "call_function",
    "classmethoddescr_call",
    "cmpwrapper_call",
    "do_call_core",
    "fast_function",
    "function_call",
    "function_code_fastcall",
    "instance_call",
    "instancemethod_call",
    "instancemethod_call",
    "methoddescr_call",
    "proxy_call",
    "slot_tp_call",
    "trace_call_function",
    "type_call",
    "weakref_call",
    "wrap_call",
    "wrapper_call",
    "wrapperdescr_call",
}

SYMBOL_BLACKLIST = {
    "call_function.lto_priv.0",
    "_PyFunction_FastCallKeywords",
    "_PyCFunction_FastCallKeywords",
    "_PyMethodDef_RawFastCallKeywords",
    "PyImport_ImportModuleLevelObject.localalias",
    "_PyObject_CallMethodIdObjArgs",
    "object_vacall",
    "_PyFunction_FastCallDict",
    "_find_and_load",
    "_find_and_load_unlocked",
    "_load_unlocked",
    "exec_module",
    "_call_with_frames_removed",
    "_PyCFunction_FastCallDict",
    "_PyMethodDef_RawFastCallDict",
    "_PyEval_EvalCodeWithName",
    "builtin_exec",
    "PyEval_EvalCode",
    "PyEval_EvalCodeEx",
    "_handle_fromlist",
    "PyCFunction_Call",
    "builtin___import__",
    "get_code",
    "_compile_bytecode",
    "marshal_loads.lto_priv.0",
    "r_object.lto_priv.0",
}

def is_frame_boring(frame: StackFrame) -> bool:  # possibly lead to lack of 'litte' functions
    return False
    function, file, *_ = frame
    if "frozen" in function or "lto_priv.0" in function or "isra.0" in function or "_PyObject" in function or \
        "_PyFrame" in function or "Py" in function:
        return True

    if "frozen" in file:
        return True

    if function in SYMBOL_BLACKLIST or (function in SYMBOL_IGNORELIST):
        return True
    return False


def is_cpython_internal(frame: StackFrame) -> bool:
    symbol, file, *_ = frame

    def _is_candidate() -> bool:
        if "PyEval_EvalFrameEx" in symbol or "_PyEval_EvalFrameDefault" in symbol:
            return True
        if symbol.startswith(("PyEval", "_Py")):
            return True
        if "vectorcall" in symbol.lower():
            return True
        if symbol in SYMBOL_IGNORELIST:
            return True
        if "Objects/call.c" in file:
            return True
        return False

    if _is_candidate():
        return re.search(RE_CPYTHON_PATHS, file) is not None
    return False


def is_frame_interesting(frame: StackFrame) -> bool:
    function, file, *_ = frame

    if file.endswith("runpy.py"):
        return False

    return not is_cpython_internal(frame)
