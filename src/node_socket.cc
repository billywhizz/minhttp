#include <node.h>
#include <node_buffer.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define MAX_CONTEXTS 65536
#define READ_BUFFER 64 * 1024

using namespace v8;
using namespace node;

namespace node {

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

typedef struct {
  int fd;
  uv_stream_t* handle;
  void* sock;
} _context;

typedef struct {
  int onConnect;
  int onChunk;
  int onWrite;
  int onError;
  int onClose;
} _callbacks;

uv_loop_t *loop;
static _context* contexts[MAX_CONTEXTS];

static Persistent<String> in_sym;
static Persistent<String> out_sym;
static Persistent<String> contexts_sym;
static Persistent<String> on_connect_sym;
static Persistent<String> on_chunk_sym;
static Persistent<String> on_write_sym;
static Persistent<String> on_error_sym;
static Persistent<String> on_close_sym;

static Persistent<FunctionTemplate> constructor_template;

class Socket : public ObjectWrap {
  private:
    _callbacks cb;
    uv_buf_t buf;
    char* _in;
    char* _out;
    Persistent<Object> _In;
    Persistent<Object> _Out;
    Persistent<Function> onConnect;
    Persistent<Function> onChunk;
    Persistent<Function> onClose;
    Persistent<Function> onWrite;
    Persistent<Function> onError;
    
    static void context_init (Socket* server, uv_stream_t* handle) {
#if NODE_MODULE_VERSION > 10
      int fd = handle->io_watcher.fd;
#else
      int fd = handle->fd;
#endif
      _context* context;
      if(!contexts[fd]) {
        context = (_context*)calloc(sizeof(_context), 1);
        context->fd = fd;
        context->sock = server;
        contexts[fd] = context;
      }
      else {
        context = contexts[fd];
      }
      context->handle = handle;
      handle->data = context;
    }

    static void context_free (uv_handle_t* handle) {
      free(handle);
    }

    static void after_write(uv_write_t* req, int status) {
      write_req_t* wr;
      if (status) {
        uv_err_t err = uv_last_error(loop);
        fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
      }
      wr = (write_req_t*) req;
      uv_stream_t* s = (uv_stream_t*)wr->req.handle;
#if NODE_MODULE_VERSION > 10
      _context* ctx = contexts[s->io_watcher.fd];
#else
      _context* ctx = contexts[s->fd];
#endif
      Socket* socket = static_cast<Socket*>(ctx->sock);
      if(socket->cb.onWrite) {
        HandleScope scope;
        Local<Value> argv[3] = { Integer::New(ctx->fd), Integer::New(wr->req.write_index), Integer::New(status) };
        socket->onWrite->Call(socket->handle_, 3, argv);
      }
      free(wr->buf.base);
      free(wr);
    }

    static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
      if (nread < 0) {
        uv_close((uv_handle_t*)handle, on_close);
        return;
      }
      if (nread == 0) {
        return;
      }
      _context* ctx = (_context*)handle->data;
      Socket* s = static_cast<Socket*>(ctx->sock);
      if(s->cb.onChunk) {
        HandleScope scope;
        memcpy(s->_in, buf.base, nread);
        Local<Value> argv[2] = { Integer::New(ctx->fd), Integer::New(nread) };
        s->onChunk->Call(s->handle_, 2, argv);
      }
    }
    
    static uv_buf_t echo_alloc(uv_handle_t* handle, size_t suggested_size) {
      _context* ctx = (_context*)handle->data;
      Socket* s = static_cast<Socket*>(ctx->sock);
      return s->buf;
    }

    static void on_close(uv_handle_t* peer) {
      _context* ctx = (_context*)peer->data;
      Socket* s = static_cast<Socket*>(ctx->sock);
      if(s->cb.onClose) {
        HandleScope scope;
        Local<Value> argv[1] = { Integer::New(ctx->fd) };
        s->onClose->Call(s->handle_, 1, argv);
      }
      context_free(peer);
    }
    
    static void after_shutdown(uv_shutdown_t* req, int status) {
      uv_close((uv_handle_t*)req->handle, on_close);
      free(req);
    }
    
    static void on_server_connection(uv_stream_t* server, int status) {
      Socket* s = static_cast<Socket*>(server->data);
      uv_stream_t* stream;
      int r;
      if (status != 0) {
        fprintf(stderr, "Connect error %d\n", uv_last_error(loop).code);
      }
      assert(status == 0);
      stream = (uv_stream_t*)malloc(sizeof(uv_tcp_t));
      assert(stream != NULL);
      r = uv_tcp_init(loop, (uv_tcp_t*)stream);
      assert(r == 0);
      r = uv_accept(server, stream);
      assert(r == 0);
      context_init(s, stream);
      _context* ctx = (_context*)stream->data;
      r = uv_read_start(stream, echo_alloc, after_read);
      assert(r == 0);
      if(s->cb.onConnect) {
        HandleScope scope;
        Local<Value> argv[1] = { Integer::New(ctx->fd) };
        s->onConnect->Call(s->handle_, 1, argv);
      }
    }

    static void on_client_connection(uv_connect_t* client, int status) {
      Socket* s = static_cast<Socket*>(client->handle->data);
      int r;
      if (status != 0) {
        fprintf(stderr, "Connect error %d\n", uv_last_error(loop).code);
      }
      assert(status == 0);
      assert(uv_is_readable(client->handle));
      assert(uv_is_writable(client->handle));
      assert(!uv_is_closing((uv_handle_t *)client->handle));
      context_init(s, client->handle);
      _context* ctx = (_context*)client->handle->data;
      r = uv_read_start(client->handle, echo_alloc, after_read);
      assert(r == 0);
      if(s->cb.onConnect) {
        HandleScope scope;
        Local<Value> argv[1] = { Integer::New(ctx->fd) };
        s->onConnect->Call(s->handle_, 1, argv);
      }
    }

  public:
    static Handle<Value> GetIn(Local<String> property, const v8::AccessorInfo& info) {
      Socket *s = ObjectWrap::Unwrap<Socket>(info.This());
      return s->_In;
    }

    static void SetIn(Local<String> property, Local<Value> value, const v8::AccessorInfo& info) {
      HandleScope scope;
      Socket *s = ObjectWrap::Unwrap<Socket>(info.This());
      Local<Object> buffer_obj = value->ToObject();
      s->_in = Buffer::Data(buffer_obj);
      s->_In = Persistent<Object>::New(buffer_obj);
    }

    static Handle<Value> GetOut(Local<String> property, const v8::AccessorInfo& info) {
      Socket *s = ObjectWrap::Unwrap<Socket>(info.This());
      return s->_Out;
    }

    static void SetOut(Local<String> property, Local<Value> value, const v8::AccessorInfo& info) {
      HandleScope scope;
      Socket *s = ObjectWrap::Unwrap<Socket>(info.This());
      Local<Object> buffer_obj = value->ToObject();
      s->_out = Buffer::Data(buffer_obj);
      s->_Out = Persistent<Object>::New(buffer_obj);
    }

    static void Initialize (v8::Handle<v8::Object> target)
    {
      HandleScope scope;
      Local<FunctionTemplate> t = FunctionTemplate::New(Socket::New);
      constructor_template = Persistent<FunctionTemplate>::New(t);
      t->InstanceTemplate()->SetInternalFieldCount(1);
      t->SetClassName(String::NewSymbol("Socket"));

      on_connect_sym = NODE_PSYMBOL("onConnect");
      on_chunk_sym = NODE_PSYMBOL("onChunk");
      on_write_sym = NODE_PSYMBOL("onWrite");
      on_error_sym = NODE_PSYMBOL("onError");
      on_close_sym = NODE_PSYMBOL("onClose");

      in_sym = NODE_PSYMBOL("in");
      out_sym = NODE_PSYMBOL("out");

      NODE_SET_PROTOTYPE_METHOD(t, "listen", Socket::Listen);
      NODE_SET_PROTOTYPE_METHOD(t, "write", Socket::Write);
      NODE_SET_PROTOTYPE_METHOD(t, "close", Socket::Close);
      NODE_SET_PROTOTYPE_METHOD(t, "pause", Socket::Pause);
      NODE_SET_PROTOTYPE_METHOD(t, "resume", Socket::Resume);
      NODE_SET_PROTOTYPE_METHOD(t, "setCallbacks", Socket::BindCallbacks);
      NODE_SET_PROTOTYPE_METHOD(t, "setNoDelay", Socket::SetNoDelay);
      NODE_SET_PROTOTYPE_METHOD(t, "setKeepAlive", Socket::SetKeepAlive);
      NODE_SET_PROTOTYPE_METHOD(t, "connect", Socket::Connect);

      t->InstanceTemplate()->SetAccessor(in_sym, Socket::GetIn, Socket::SetIn);
      t->InstanceTemplate()->SetAccessor(out_sym, Socket::GetOut, Socket::SetOut);
      
      target->Set(String::NewSymbol("Socket"), t->GetFunction());
    }

  protected:
    static Handle<Value> New (const Arguments& args)
    {
      HandleScope scope;
      Socket *server = new Socket();
      server->cb.onClose = server->cb.onError = server->cb.onChunk = server->cb.onWrite = server->cb.onConnect = 0;
      server->Wrap(args.Holder());
      server->Ref();
      server->buf = uv_buf_init((char*)malloc(READ_BUFFER), READ_BUFFER);
      return args.This();
    }

    static Handle<Value> BindCallbacks(const Arguments &args) {
      HandleScope scope;
      Socket *s = ObjectWrap::Unwrap<Socket>(args.Holder());
      s->cb.onConnect = 0;
      if(s->handle_->HasOwnProperty(on_connect_sym)) {
        Local<Value> onConnect = s->handle_->Get(on_connect_sym);
        if (onConnect->IsFunction()) {
          s->onConnect = Persistent<Function>::New(Local<Function>::Cast(onConnect));
          s->cb.onConnect = 1;
        }
      }
      s->cb.onChunk = 0;
      if(s->handle_->HasOwnProperty(on_chunk_sym)) {
        Local<Value> onChunk = s->handle_->Get(on_chunk_sym);
        if (onChunk->IsFunction()) {
          s->onChunk = Persistent<Function>::New(Local<Function>::Cast(onChunk));
          s->cb.onChunk = 1;
        }
      }
      s->cb.onWrite = 0;
      if(s->handle_->HasOwnProperty(on_write_sym)) {
        Local<Value> onWrite = s->handle_->Get(on_write_sym);
        if (onWrite->IsFunction()) {
          s->onWrite = Persistent<Function>::New(Local<Function>::Cast(onWrite));
          s->cb.onWrite = 1;
        }
      }
      s->cb.onClose = 0;
      if(s->handle_->HasOwnProperty(on_close_sym)) {
        Local<Value> onClose = s->handle_->Get(on_close_sym);
        if (onClose->IsFunction()) {
          s->onClose = Persistent<Function>::New(Local<Function>::Cast(onClose));
          s->cb.onClose = 1;
        }
      }
      s->cb.onError = 0;
      if(s->handle_->HasOwnProperty(on_error_sym)) {
        Local<Value> onError = s->handle_->Get(on_error_sym);
        if (onError->IsFunction()) {
          s->onError = Persistent<Function>::New(Local<Function>::Cast(onError));
          s->cb.onError = 1;
        }
      }
      return Null();
    }
    
    static Handle<Value> Write(const Arguments &args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      int off = args[1]->Int32Value();
      int len = args[2]->Int32Value();
      _context* ctx = contexts[fd];
      write_req_t *wr;
      wr = (write_req_t*) malloc(sizeof *wr);
      Socket* s = static_cast<Socket*>(ctx->sock);
      char* towrite = (char*)malloc(len);
      char* src = s->_out + off;
      memcpy(towrite, src, len);
      wr->buf = uv_buf_init(towrite, len);
      if (uv_write(&wr->req, ctx->handle, &wr->buf, 1, after_write)) {
        fprintf(stderr, "error\n");
        exit(1);
      }
      return scope.Close(Integer::New(wr->req.write_index));
    }

    static Handle<Value> Close(const Arguments &args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      _context* ctx = contexts[fd];
      uv_shutdown_t* req;
      req = (uv_shutdown_t*) malloc(sizeof *req);
      uv_shutdown(req, ctx->handle, after_shutdown);
      return scope.Close(Integer::New(1));
    }

    static Handle<Value> Pause(const Arguments &args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      _context* ctx = contexts[fd];
      int r = uv_read_stop(ctx->handle);
      if (r) SetErrno(uv_last_error(uv_default_loop()));
      return scope.Close(Integer::New(r));
    }

    static Handle<Value> Resume(const Arguments &args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      _context* ctx = contexts[fd];
      int r = uv_read_start(ctx->handle, echo_alloc, after_read);
      if (r) SetErrno(uv_last_error(uv_default_loop()));
      return scope.Close(Integer::New(r));
    }

    static Handle<Value> SetNoDelay(const Arguments& args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      _context* ctx = contexts[fd];
      int enable = static_cast<int>(args[1]->BooleanValue());
      int r = uv_tcp_nodelay((uv_tcp_t*)ctx->handle, enable);
      if (r) SetErrno(uv_last_error(uv_default_loop()));
      return scope.Close(Integer::New(r));
    }

    static Handle<Value> SetKeepAlive(const Arguments& args) {
      HandleScope scope;
      int fd = args[0]->Int32Value();
      _context* ctx = contexts[fd];
      int enable = static_cast<int>(args[1]->BooleanValue());
      unsigned int delay = args[2]->Uint32Value();
      int r = uv_tcp_keepalive((uv_tcp_t*)ctx->handle, enable, delay);
      if (r) SetErrno(uv_last_error(uv_default_loop()));
      return scope.Close(Integer::New(r));
    }

    static Handle<Value> Connect(const Arguments& args) {
      HandleScope scope;
      String::AsciiValue ip_address(args[0]);
      int port = args[1]->Int32Value();
      Socket *s = ObjectWrap::Unwrap<Socket>(args.Holder());
      struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
      uv_tcp_t* sock = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
      sock->data = s;
      int r = uv_tcp_init(uv_default_loop(), sock);
      uv_connect_t* cn_wrap = (uv_connect_t*)malloc(sizeof(uv_connect_t));
      r = uv_tcp_connect(cn_wrap, sock, address, on_client_connection);
      if (r) {
        SetErrno(uv_last_error(uv_default_loop()));
        free(cn_wrap);
      }
      return scope.Close(Integer::New(r));
    }

    static Handle<Value> Listen(const Arguments &args) {
      HandleScope scope;
      Socket *s = ObjectWrap::Unwrap<Socket>(args.Holder());
      String::AsciiValue ip_address(args[0]->ToString());
      int port = args[1]->Int32Value();
      loop = uv_default_loop();
      uv_tcp_t* sock = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
      sock->data = s;
      struct sockaddr_in addr = uv_ip4_addr(*ip_address, port);
      int r = 0;
      r = uv_tcp_init(loop, sock);
      if (r) {
        fprintf(stderr, "Socket creation error\n");
        r = -1;
      }
      else {
        r = uv_tcp_bind(sock, addr);
        if (r) {
          fprintf(stderr, "Bind error\n");
          r = -2;
        }
        else {
          r = uv_listen((uv_stream_t*)sock, SOMAXCONN, on_server_connection);
          if (r) {
            fprintf(stderr, "Listen error\n");
            r = -3;
          }
        }
      }
      return scope.Close(Integer::New(r));
    }

    Socket () : ObjectWrap () 
    {
    }

    ~Socket ()
    {
    }
};
}
NODE_MODULE(socket, node::Socket::Initialize)