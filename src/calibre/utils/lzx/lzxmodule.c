/* __license__   = 'GPL v3'
 * __copyright__ = '2008, Marshall T. Vandegrift <llasram@gmail.com>'
 *
 * Python module C glue code.
 */


#include <Python.h>

#include <mspack.h>
#include <lzx.h>

static char lzx_doc[] = 
"Provide basic LZX decompression using the code from libmspack.";

static PyObject *LzxError = NULL;

typedef struct memory_file {
    unsigned int magic;	/* 0xB5 */
    void * buffer;
    int total_bytes;
    int current_bytes;
} memory_file;

void *
glue_alloc(struct mspack_system *this, size_t bytes)
{
    void *p = NULL;
    p = (void *)malloc(bytes);
    if (p == NULL) {
        return (void *)PyErr_NoMemory();
    }
    return p;
}

void
glue_free(void *p)
{
    free(p);
}

void
glue_copy(void *src, void *dest, size_t bytes)
{
    memcpy(dest, src, bytes);
}

struct mspack_file *
glue_open(struct mspack_system *this, char *filename, int mode)
{
    PyErr_SetString(LzxError, "MSPACK_OPEN unsupported");
    return NULL;
}

void
glue_close(struct mspack_file *file)
{
    return;
}

int
glue_read(struct mspack_file *file, void * buffer, int bytes)
{
    memory_file *mem;
    int remaining;

    mem = (memory_file *)file;
    if (mem->magic != 0xB5) return -1;
  
    remaining = mem->total_bytes - mem->current_bytes;
    if (!remaining) return 0;
    if (bytes > remaining) bytes = remaining;
    memcpy(buffer, (unsigned char *)mem->buffer + mem->current_bytes, bytes);
    mem->current_bytes += bytes;
    
    return bytes;
}

int
glue_write(struct mspack_file * file, void * buffer, int bytes)
{
    memory_file *mem;
    int remaining;

    mem = (memory_file *)file;
    if (mem->magic != 0xB5) return -1;
  
    remaining = mem->total_bytes - mem->current_bytes;
    if (!remaining)  return 0;
    if (bytes > remaining) {
        PyErr_SetString(LzxError,
            "MSPACK_WRITE tried to write beyond end of buffer");
        bytes = remaining;
    }
    memcpy((unsigned char *)mem->buffer + mem->current_bytes, buffer, bytes);
    mem->current_bytes += bytes;
    return bytes;
}

struct mspack_system lzxglue_system = {
    glue_open, 
    glue_close,
    glue_read,   /* Read */
    glue_write,  /* Write */
    NULL,        /* Seek */
    NULL,        /* Tell */
    NULL,        /* Message */
    glue_alloc,
    glue_free,
    glue_copy,
    NULL         /* Termination */
};


int LZXwindow = 0;
struct lzxd_stream * lzx_stream = NULL;

/* Can't really init here, don't know enough */
static PyObject *
init(PyObject *self, PyObject *args)
{
    int window = 0;

    if (!PyArg_ParseTuple(args, "i", &window)) {
        return NULL;
    }
    
    LZXwindow = window;
    lzx_stream = NULL;

    Py_RETURN_NONE;
}

/* Doesn't exist.  Oh well, reinitialize state every time anyway */
static PyObject *
reset(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }

    Py_RETURN_NONE;
}

//int LZXdecompress(unsigned char *inbuf, unsigned char *outbuf, 
//    unsigned int inlen, unsigned int outlen)
static PyObject *
decompress(PyObject *self, PyObject *args)
{
    unsigned char *inbuf;
    unsigned char *outbuf;
    unsigned int inlen;
    unsigned int outlen;
    int err;
    memory_file source;
    memory_file dest;
    PyObject *retval = NULL;

    if (!PyArg_ParseTuple(args, "s#I", &inbuf, &inlen, &outlen)) {
        return NULL;
    }

    retval = PyString_FromStringAndSize(NULL, outlen);
    if (retval == NULL) {
        return NULL;
    }
    outbuf = (unsigned char *)PyString_AS_STRING(retval);
    
    source.magic = 0xB5;
    source.buffer = inbuf;
    source.current_bytes = 0;
    source.total_bytes = inlen;

    dest.magic = 0xB5;
    dest.buffer = outbuf;
    dest.current_bytes = 0;
    dest.total_bytes = outlen;
    
    lzx_stream = lzxd_init(&lzxglue_system, (struct mspack_file *)&source,
        (struct mspack_file *)&dest, LZXwindow, 
        0x7fff /* Never reset, I do it */, 4096, outlen);
    err = -1;
    if (lzx_stream) err = lzxd_decompress(lzx_stream, outlen);

    lzxd_free(lzx_stream);
    lzx_stream = NULL;

    if (err != MSPACK_ERR_OK) {
        Py_DECREF(retval);
        PyErr_SetString(LzxError, "LZX decompression failed");
    }
    
    return retval;
}

static PyMethodDef lzx_methods[] = {
    { "init", &init, METH_VARARGS, "Initialize the LZX decompressor" },
    { "reset", &reset, METH_VARARGS, "Reset the LZX decompressor" },
    { "decompress", &decompress, METH_VARARGS, "Run the LZX decompressor" },
    { NULL, NULL }
};

PyMODINIT_FUNC
initlzx(void)
{
    PyObject *m;

    m = Py_InitModule3("lzx", lzx_methods, lzx_doc);
    if (m == NULL) {
        return;
    }
    
    LzxError = PyErr_NewException("lzx.LzxError", NULL, NULL);
    Py_INCREF(LzxError);
    PyModule_AddObject(m, "LzxError", LzxError);
    
    return;
}
