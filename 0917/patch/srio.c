--- spark_srio/fvl_srio.c	2015-04-23 23:52:26.000000000 +0800
+++ new_srio/fvl_srio.c	2015-04-23 23:56:27.000000000 +0800
@@ -21,6 +21,8 @@
 static uint64_t rfreeback_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0};
 static uint64_t read_num[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
 
+static uint64_t error_count[FVL_CHAN_NUM_MAX]={0,0,0,0,0,0,0,0};
+
 static fvl_srio_ctable_t srio_ctable_context[FVL_CHAN_NUM_MAX]={
 {"srio0-chan0",0,0,0},{"srio0-chan1",0,1,0},{"srio0-chan2",0,2,0},{"srio0-chan3",0,3,0},
 {"srio1-chan0",1,0,0},{"srio1-chan1",1,1,0},{"srio1-chan2",1,2,0},{"srio1-chan3",1,3,0}
@@ -69,7 +71,7 @@
     }
     else if(srio_param->mode==1)
     {
-        uint64_t vs_type=srio_param->version_mode;
+        uint32_t vs_type=srio_param->version_mode;
         version_mode[port_num]=vs_type;
     	rd_op_base[port_num]=2;
     	wr_op_base[port_num]=1;
@@ -537,7 +539,12 @@
     pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num+7];
 
     pthread_mutex_lock(&mutex[port_num]); 
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+    rvl = fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+    if(rvl!=0)
+    {
+        error_count[port_num]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[port_num]);
+    }
     pthread_mutex_unlock(&mutex[port_num]);
 
     FVL_LOG("PORT_NUM:%d HEAD_INFO_SIZE:256\n",port_num);
@@ -701,7 +708,12 @@
             pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num];
             src_phys= ppool->write_ctl_data;
             dest_phys= ppool->ctl_info_start;
-            fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+            rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+            if(rvl!=0)
+            {
+                error_count[port_num]++;
+                FVL_LOG("DMA do't response times:%lu\n",error_count[port_num]);
+            }
             port_uflag[port_num]=1;
             FVL_LOG("port_num:%d Receive Head info and send chan num info sucess!\n",port_num);
         }
@@ -733,7 +745,7 @@
                 continue;
             }
             pcnt->cmd_ack=0;
-            uint64_t vs_type=pcnt->vs_type;
+            uint32_t vs_type=pcnt->vs_type;
 
             if(vs_type & 0x01)
             {
@@ -796,7 +808,7 @@
                 continue;
             }
             pcnt->cmd_ack=0;
-            uint64_t vs_type=pcnt->vs_type;
+            uint32_t vs_type=pcnt->vs_type;
             if(vs_type & 0x01)
             {
             	second_handshake[priv->num]=1;
@@ -1061,8 +1073,12 @@
     FVL_LOG("Head size:%08x FVL:%08x cmd_ack:%08x\n",HEAD_SIZE[port_num],FVL_CTL_HEAD_SIZE,head_channel_response[fd].cmd_ack);
 
     memcpy(cpool->pwrite_ctl_data,&head_channel_response[fd],sizeof(head_channel_response[fd]));
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
-
+    rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+    if(rvl!=0)
+    {
+        error_count[fd]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[fd]);
+    }
     FVL_LOG("##### channel:%d Slave reback ctl_head info!\n",fd);
 
 // add
@@ -1252,7 +1268,7 @@
     pscb=&(temp_channel->chanblk);
     cpool=&(temp_channel->chanpool);
     uint64_t dest_phys,src_phys;
-    int port_num=0;
+    int port_num=0,rvl=0;
     uint32_t buf_size=0,buf_num=0;
     uint32_t offset=0;
     uint32_t step=0;
@@ -1288,14 +1304,23 @@
         }
         dest_phys=(uint64_t )(cpool->ctl_info_start+ctl_offset);
         src_phys =(uint64_t )(cpool->write_ctl_data+ctl_offset);
-        fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+        rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+        if(rvl!=0)
+        {
+            error_count[fd]++;
+            FVL_LOG("DMA do't response times:%lu\n",error_count[fd]);
+        }
         return -1;
     }
-
     offset=buf_size*(send_num[fd]%buf_num);
     dest_phys=(uint64_t )(cpool->port_info.range_start+offset);
     src_phys = phys; 
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,length);
+    rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,length);
+    if(rvl!=0)
+    {
+        error_count[fd]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[fd]);
+    }
     
     send_num[fd]=send_num[fd]+step;
     pcnt=(fvl_srio_ctl_info_t *)(cpool->pwrite_ctl_data+ctl_offset);
@@ -1311,7 +1336,12 @@
     
     dest_phys=(uint64_t )(cpool->ctl_info_start+ctl_offset);
     src_phys =(uint64_t )(cpool->write_ctl_data+ctl_offset);
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+    rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+    if(rvl!=0)
+    {
+        error_count[fd]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[fd]);
+    }
     
     return 0;
 }
@@ -1387,7 +1417,7 @@
     pscb=&(temp_channel->rechanblk);
     cpool=&(temp_channel->chanpool);
     uint64_t dest_phys,src_phys;
-    int port_num=0,buf_num=0;
+    int port_num=0,buf_num=0,rvl=0;
     port_num=srio_ctable_context[fd].port;
     buf_num  = srio_channel_context[fd].buf_num;
     
@@ -1410,7 +1440,12 @@
     {
         pthread_mutex_lock(&mutex[port_num]); 
     }
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+    rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,HEAD_SIZE[port_num]);
+    if(rvl!=0)
+    {
+        error_count[fd]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[fd]);
+    }
     if(Index==(FVL_PORT_CHAN_NUM_MAX-1))
     {
         pthread_mutex_unlock(&mutex[port_num]); 
@@ -1478,7 +1513,12 @@
     pscb=&psrio->ctrlblk[FVL_PORT_DMA_NUM*port_num+7];
 
     pthread_mutex_lock(&mutex[port_num]); 
-    fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+    rvl=fvl_srio_send(pscb->dmadev,src_phys,dest_phys,256);
+    if(rvl!=0)
+    {
+        error_count[port_num]++;
+        FVL_LOG("DMA do't response times:%lu\n",error_count[port_num]);
+    }
     pthread_mutex_unlock(&mutex[port_num]);
 
     head_op.num = port_num;
