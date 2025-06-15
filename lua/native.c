#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <assert.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <mbedtls/base64.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include "../test/cacert.inc"

int EncodeBase64(lua_State *L) {
  const char *dec = luaL_checkstring(L, 1);
  size_t len = lua_rawlen(L, 1), outlen;
  char *enc;
  mbedtls_base64_encode(NULL, 0, &outlen, dec, len);
  if (!(enc = malloc(outlen))) return luaL_error(L, "Allocation failed");
  assert(mbedtls_base64_encode(enc, outlen, &outlen, dec, len) == 0);
  lua_pushstring(L, enc);
  free(enc);
  return 1;
}

int DecodeBase64(lua_State *L) {
  const char *enc = luaL_checkstring(L, 1);
  size_t len = lua_rawlen(L, 1), outlen;
  char *dec;
  mbedtls_base64_decode(NULL, 0, &outlen, enc, len);
  if (!(dec = malloc(outlen))) return luaL_error(L, "Allocation failed");
  if (mbedtls_base64_decode(dec, outlen, &outlen, enc, len) == 0) {
    lua_pushstring(L, dec);
    free(dec);
    return 1;
  } else {
    free(dec);
    lua_pushnil(L);
    lua_pushstring(L, "mbedtls: base64 decoding failed");
    return 2;
  }
}

static struct {
  mbedtls_ssl_context ssl;
  mbedtls_net_context server_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_x509_crt cacert;
  mbedtls_ssl_config conf;
} conn;

static void InitializeConn(const char *server, const char *hostname, const char *port) {
  mbedtls_ssl_init(&conn.ssl);
  mbedtls_x509_crt_init(&conn.cacert);
  mbedtls_ctr_drbg_init(&conn.ctr_drbg);
  mbedtls_ssl_config_init(&conn.conf);
  mbedtls_entropy_init(&conn.entropy);
  assert(mbedtls_ctr_drbg_seed(&conn.ctr_drbg, mbedtls_entropy_func,
                               &conn.entropy, NULL, 0) == 0);
  //assert(mbedtls_x509_crt_parse_file(
  //           &conn.cacert, "/etc/ssl/certs/ca-certificates.crt") >= 0);
  assert(mbedtls_x509_crt_parse(&conn.cacert, cacert_pem, cacert_pem_len) >=
         0);
  assert(mbedtls_ssl_set_hostname(&conn.ssl, hostname) == 0);
  assert(mbedtls_ssl_config_defaults(&conn.conf, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT) == 0);
  mbedtls_ssl_conf_authmode(&conn.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&conn.conf, &conn.cacert, NULL);
  mbedtls_ssl_conf_rng(&conn.conf, mbedtls_ctr_drbg_random,
                       &conn.ctr_drbg);
  mbedtls_ssl_conf_max_tls_version(&conn.conf, MBEDTLS_SSL_VERSION_TLS1_2);
  assert(mbedtls_ssl_setup(&conn.ssl, &conn.conf) == 0);
  mbedtls_net_init(&conn.server_fd);
  assert(mbedtls_net_connect(&conn.server_fd, server, port,
                             MBEDTLS_NET_PROTO_TCP) == 0);
  mbedtls_ssl_set_bio(&conn.ssl, &conn.server_fd, mbedtls_net_send,
                      mbedtls_net_recv, NULL);
}

static int CloseConnection(lua_State *L) {
  mbedtls_ssl_close_notify(&conn.ssl);
  mbedtls_net_free(&conn.server_fd);
  mbedtls_x509_crt_free(&conn.cacert);
  mbedtls_ssl_free(&conn.ssl);
  mbedtls_ssl_config_free(&conn.conf);
  mbedtls_ctr_drbg_free(&conn.ctr_drbg);
  mbedtls_entropy_free(&conn.entropy);
  return 0;
}

static void Handshake() {
  int r;
  while ((r = mbedtls_ssl_handshake(&conn.ssl)) != 0)
    assert(r == MBEDTLS_ERR_SSL_WANT_READ ||
           r == MBEDTLS_ERR_SSL_WANT_WRITE);
  assert(mbedtls_ssl_get_verify_result(&conn.ssl) == 0);
}

int Connect(lua_State *L) {
  const char *ip = luaL_checkstring(L, 1);
  const char *hostname = luaL_checkstring(L, 2);
  const char *port = luaL_checkstring(L, 3);
  InitializeConn(ip, hostname, port);
  return 0;
}

int Send(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  size_t n = lua_rawlen(L, 1);
  int sent;
  if (0) // TODO: tls
    sent = mbedtls_ssl_write(&conn.ssl, s, n);
  else
    sent = mbedtls_net_send(&conn.server_fd, s, n);
  // TODO: send all
  assert(sent >= 0);
}

static uint8_t recvbuf[10000];

static void Receive(lua_State *L) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  // TODO: receive more
  int n = mbedtls_net_recv(&conn.server_fd, recvbuf, sizeof(recvbuf));
  luaL_addlstring(&b, recvbuf, n);
  luaL_pushresult(&b);
}

int EventLoop(lua_State *L) {
  while (1) {
    struct pollfd fds[2] = {0};
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = conn.server_fd.fd;
    fds[1].events = POLLIN;
    puts("POLLING");
    int r = poll(fds, 2, -1);
    puts("GOT");
    if (fds[0].revents & POLLIN) {
      lua_getglobal(L, "OnStdin");
      if (lua_isfunction(L, -1)) {
        lua_pushstring(L, "some input!");
        lua_call(L, 1, 0);
      } else {
        return luaL_error(L, "OnStdin is not a function");
      }
    }
    if (fds[1].revents & POLLIN) {
      lua_getglobal(L, "OnReceive");
      if (lua_isfunction(L, -1)) {
        Receive(L);
        lua_call(L, 1, 0);
      } else {
        return luaL_error(L, "OnReceive is not a function");
      }
    }
  }
}

int luaopen_native(lua_State *L) {
  lua_register(L, "EncodeBase64", EncodeBase64);
  lua_register(L, "DecodeBase64", DecodeBase64);
  lua_register(L, "EventLoop", EventLoop);
  lua_register(L, "Connect", Connect);
  lua_register(L, "Send", Send);
  lua_register(L, "CloseConnection", Connect);
  return 0;
}

