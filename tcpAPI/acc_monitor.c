#define OS_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <Python.h>
#include "tcp_transfer.h"
#include "fpga-sim/driver/fpga-libacc.h"


static PyObject* start_service(PyObject *self, PyObject *args)
{
    PyObject * ret;
    const char *section_id, *c_in_buf_size, *c_out_buf_size, *c_acc_name;

    if (!PyArg_ParseTuple(args, "ssss", &section_id, &c_in_buf_size, &c_out_buf_size, &c_acc_name)){
    	return NULL;
    }

    struct server_param_t server_param;
    memset((void *) &server_param, 0, sizeof(server_param));

    strcpy(server_param.acc_name, c_acc_name);
    server_param.in_buf_size = atoi(c_in_buf_size);
    server_param.out_buf_size = atoi(c_out_buf_size);

    strcpy(server_param.section_id, section_id); 

    

    //open a socket server and local acc_slot;
    socket_server_open((void *) &server_param);


    //return port number and open_status(success or failure)back to client.
    ret = Py_BuildValue("{s:s, s:s, s:s}", "ip", server_param.ipaddr, "port", server_param.port, "ifuse", server_param.status);
    //printf("ip = %s, port =%s, ifuse = %s\n", server_param.ipaddr, server_param.port, server_param.status);
    return ret;
}

static PyMethodDef DemoMethods[] = {
    {"start_service",  start_service, METH_VARARGS,
     "acc_monitor function"},
    {NULL, NULL, 0, NULL}
};

static PyObject *DemoError;
static char name[100];

PyMODINIT_FUNC initacc_monitor(void)
{
    PyObject *m;

    m = Py_InitModule("acc_monitor", DemoMethods);

    if (m == NULL)
        return;

    strcpy(name, "acc_monitor.error");
    DemoError = PyErr_NewException(name, NULL, NULL);
    Py_INCREF(DemoError);
    strcpy(name, "error");
    PyModule_AddObject(m, name, DemoError);
}


int main(int argc, char *argv[])
{
    Py_SetProgramName(argv[0]);
    Py_Initialize();
    initacc_monitor();
    return 1;
}
