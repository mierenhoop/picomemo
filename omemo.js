if (typeof mergeInto !== 'undefined') {
  mergeInto(LibraryManager.library, {
    omemoJsLoadMessageKey: function() {return 0;},
    omemoJsStoreMessageKey: function() {return 0;},
    omemoJsRandom: function(p,n) {
      let crypto = require("node:crypto")
      let buf = new Uint8Array(n)
      crypto.getRandomValues(buf)
      for (let i = 0; i < n; i++) {
        HEAPU8[p+i] = buf[i];
      }
    },
  });
}
