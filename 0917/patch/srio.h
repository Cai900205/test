--- spark_srio/fvl_srio.h	2015-09-17 23:46:29.000000000 +0800
+++ new_srio/fvl_srio.h	2015-04-23 23:56:27.000000000 +0800
@@ -129,7 +129,8 @@
     uint32_t symbol;
     uint32_t status;
     uint32_t cmd_ack;
-    uint64_t vs_type;
+    uint32_t vs_type0;
+    uint32_t vs_type;
 } fvl_srio_response_info_t;
 //end
 typedef struct fvl_srio_ctrlblk {
