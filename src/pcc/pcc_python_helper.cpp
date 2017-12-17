
#include "pcc_python_helper.h"

PccPythonHelper::PccPythonHelper(const std::string& python_filename) {
    std::cout << "Initializing python\n";
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/users/njay2\")");
    PyRun_SimpleString("sys.path.append(\"/home/njay2/PCC/clean_ppc2/pcc/src\")");
    std::cout << "Getting module name\n";
    PyObject* module_name = PyString_FromString(python_filename.c_str());
    if (module_name == NULL) {
        std::cerr << "ERROR: Could create module name: " << python_filename << std::endl;
        PyErr_Print();
        exit(-1);
    }
    std::cout << "Importing Module\n";
    module = PyImport_Import(module_name);
    if (module == NULL) {
        std::cerr << "ERROR: Could not load python module: " << python_filename << std::endl;
        PyErr_Print();
        exit(-1);
    }
    std::cout << "Decreasing module name references\n";
    Py_DECREF(module_name);
    std::cout << "Getting function named \"give_sample\"\n";
    give_sample_func = PyObject_GetAttrString(module, "give_sample");
    std::cout << "Getting function named \"get_rate_change\"\n";
    get_rate_change_func = PyObject_GetAttrString(module, "get_rate_change");
}

PccPythonHelper::~PccPythonHelper() {
    Py_DECREF(module);
    Py_DECREF(give_sample_func);
    Py_DECREF(get_rate_change_func);
    Py_Finalize();
}

void PccPythonHelper::GiveSample(double sending_rate, double latency, double loss_rate) {
    std::cout << "PccPythonHelper::GiveSample\n";
    PyObject* args = PyTuple_New(3);
    PyObject* sending_rate_value = PyFloat_FromDouble(sending_rate);
    PyObject* latency_value = PyFloat_FromDouble(latency);
    PyObject* loss_rate_value = PyFloat_FromDouble(loss_rate);
    PyTuple_SetItem(args, 0, sending_rate_value);
    PyTuple_SetItem(args, 1, latency_value);
    PyTuple_SetItem(args, 2, loss_rate_value);
    PyObject* result = PyObject_CallObject(give_sample_func, args);
    Py_DECREF(sending_rate_value);
    Py_DECREF(latency_value);
    Py_DECREF(loss_rate_value);
    Py_DECREF(args);
    Py_DECREF(result);
}

double PccPythonHelper::GetRateChange() {
    std::cout << "PccPythonHelper::GetRateChange\n";
    PyObject* result = PyObject_CallObject(get_rate_change_func, NULL);
    double result_double = PyFloat_AS_DOUBLE(result);
    Py_DECREF(result);
    return result_double;
}
