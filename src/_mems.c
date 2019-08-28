#include <Python.h>
#include <numpy/arrayobject.h>
#include "rosco.h"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

static char module_docstring[] =
    "This module provides an interface for reading MEMS ECU using C.";
static char mems_docstring[] =
    "Read the MEMS ECU";

static PyObject *read_mems(PyObject *self, PyObject *args);
static PyObject *connect_mems(PyObject *self, PyObject *args);

static PyMethodDef module_methods[] = {
    {"read", read_mems, METH_VARARGS, mems_docstring},
    {"connect", connect_mems, METH_VARARGS, mems_docstring},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC PyInit__mems(void)
{
    
    PyObject *module;
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_mems",
        module_docstring,
        -1,
        module_methods,
        NULL,
        NULL,
        NULL,
        NULL
    };
    module = PyModule_Create(&moduledef);
    if (!module) return NULL;

    /* Load `numpy` functionality. */
    import_array();

    return module;
}

static PyObject *read_mems(PyObject *self, PyObject *args)
{
    /* Build the output  */
    PyObject *ret = Py_BuildValue("{si}", "read", 200);
    return ret;
}

static PyObject *connect_mems(PyObject *self, PyObject *args)
{
    /* Build the output  */
    PyObject *ret = Py_BuildValue("{si}", "connect", 200);
    return ret;
}