"""Tools for processing and filtering stack frames."""
import re
from typing import Tuple
import re

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
    "PyImport_ImportModuleLevelObject.localalias",
    "object_vacall",
    "_load_unlocked",
    "exec_module",
    "_call_with_frames_removed",
    "_handle_fromlist",
    "get_code",
    "_compile_bytecode",
    "marshal_loads.lto_priv.0",
    "r_object.lto_priv.0",
    "Dispatcher_call(Dispatcher*, _object*, _object*)",
    "compile_and_invoke(Dispatcher*, _object*, _object*, _object*)",
}

BORING_KEY_STRS = {
    "PyObject_Call",
    "PyEval",
    "builtin",
    "PyMethod",
    "PyCFunction",
    "PyFunction",
    "_find_and_load",
    "partial_call",
    "copyto",
}

def is_boring_frame(frame: StackFrame, filter_boring_frame) -> bool:  # possibly lead to lack of 'litte' functions
    if filter_boring_frame == 0:
        return False
    function, file, *_ = frame
    if "frozen" in function or "lto_priv.0" in function or "isra.0" in function or "_PyObject" in function or \
        "_PyFrame" in function or "Py" in function:
        return True

    if "frozen" in file:
        return True

    if function in SYMBOL_BLACKLIST or (function in SYMBOL_IGNORELIST):
        return True

    for str in BORING_KEY_STRS:
        if function.find(str) >= 0:
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
