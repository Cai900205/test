//HDMI_I2C coded by Lu.y 2012/03/26
//version 0.1.0  12/09/27
//version 1.1.0  12/12/18 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "pcie_i2c.h"

int data = 0x13;
int rdata[20];
int ir=0;
int mytest = 0x15;
int videostyle = 0;
int configdown=0;
typedef unsigned char alt_u8;
	 
//I2C_switch
int i2c_sw_tx_w(alt_u8 data)//, alt_u8 data1)
{
    return pcie_i2c_write_switch(0xE2>>1,data);
}


//HDMI_RX
int i2c_hdmi_rx_w(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x98>>1,reg,&data,1);
}

alt_u8 i2c_hdmi_rx_r(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x98>>1, offset, &data, 1);
    return data;
}
//HDMI_RX_CP_MAP
int i2c_hdmi_rx_cp_map_w(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x44>>1,reg,&data,1);
}

alt_u8 i2c_hdmi_rx_cp_map_r(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x44>>1, offset, &data, 1);
    return data;
}
//HDMI_RX_HDMI_MAP
int i2c_hdmi_rx_hdmi_map_w(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x68>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x68>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_hdmi_map_r(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x68>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x68>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x68>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_KSV_MAP
int i2c_hdmi_rx_ksv_map_w(alt_u8 reg, alt_u8 data1)
{
#if 0
	 I2C_start(I2C_CTRL_BASE,0x64>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
    uint32_t data = data1;
    return pcie_i2c_write(0x64>>1,reg,&data,1);
}

alt_u8 i2c_hdmi_rx_ksv_map_r(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x64>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x64>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x64>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_EDID_MAP
int i2c_hdmi_rx_edid_map_w(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x6C>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6C>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_edid_map_r(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x6C>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6C>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x6C>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}


//////////////////////////////////////0x9A//////////////////////////////////
//HDMI_RX
int i2c_hdmi_rx_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x9A>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x9A>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x9A>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x9A>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x9A>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_CP_MAP
int i2c_hdmi_rx_cp_map_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x46>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x46>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_cp_map_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x46>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x46>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x46>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_HDMI_MAP
int i2c_hdmi_rx_hdmi_map_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x6A>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6A>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_hdmi_map_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x6A>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6A>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x6A>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_KSV_MAP
int i2c_hdmi_rx_ksv_map_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x66>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x66>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_ksv_map_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x66>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x66>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x66>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_EDID_MAP
int i2c_hdmi_rx_edid_map_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x6E>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6E>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_edid_map_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x6E>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x6E>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x6E>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}
//HDMI_RX_DPLL
int i2c_hdmi_rx_dpll_map_wb(alt_u8 reg, alt_u8 data1)
{
    uint32_t data = data1;
    return pcie_i2c_write(0x4E>>1,reg,&data,1);
#if 0
	 I2C_start(I2C_CTRL_BASE,0x4E>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // write register.
	data =  I2C_write(I2C_CTRL_BASE,data1,1);  // write vial.

	return data;
#endif
}

alt_u8 i2c_hdmi_rx_dpll_map_rb(alt_u8 reg)
{
    uint32_t offset = reg;
    uint32_t data;
    pcie_i2c_read(0x4E>>1, offset, &data, 1);
    return data;
#if 0
	 I2C_start(I2C_CTRL_BASE,0x4E>>1,0); //address the chip in write mode
	data =  I2C_write(I2C_CTRL_BASE,reg,0);  // set command to read input register.
	I2C_start(I2C_CTRL_BASE,0x4E>>1,1); //send start again but this time in read mode
	data =  I2C_read(I2C_CTRL_BASE,1);  // read the input register and send stop

	return data;
#endif
}

int my_delay(int mdelay)
{
    usleep(mdelay);

	return 0;
}


int main(int argc, char ** argv)
{ 

	int i;
    unsigned char md;
    unsigned char checka0,checka1,checka2;
    unsigned char checkb0,checkb1,checkb2;
    unsigned char checkc0,checkc1,checkc2;
    unsigned char checkd0,checkd1,checkd2;
    
    init_i2c_device();

 	 //=========================HDMI RX ADV9163 PHY1 config========================== 	  	    
     data = i2c_hdmi_rx_r(0xEA);//for debug
     data = i2c_hdmi_rx_r(0xEB);//for debug
	 //Main

	 //(IO)(0xFF)[7]=1:register main reset:reset 
	 //------------------------------------------------------
     md = i2c_hdmi_rx_r(0xFF);
	 md |= (1<<7);
	 i2c_hdmi_rx_w(0xFF, md);// regFF[7]=1 all register reset
	 //------------------------------------------------------
	 
	 my_delay(100);
	 
	 //(IO)(0xFF)[7]=0:register main reset:release
	 //------------------------------------------------------	 
     md = i2c_hdmi_rx_r(0xFF);
	 md &= ~(1<<7);
	 i2c_hdmi_rx_w(0xFF, md);// regFF[7]=0 reset release
	 //------------------------------------------------------
	 
     data = i2c_hdmi_rx_r(0x1B);//for debug
     
	 //(IO)(0x1B)ALSB control:change address to 0x9A
	 //------------------------------------------------------
     md = i2c_hdmi_rx_r(0x1B);
	 md |= (1<<0);
	 i2c_hdmi_rx_w(0x1B, md);// reg1B[0]=1 change device address to 0x9A
	 //------------------------------------------------------	 
	 
     //(IO)(0x0C)[5]=0;power down;pown down off chip operation 
	 //------------------------------------------------------            
	 md = i2c_hdmi_rx_rb(0x0C);
	 md &= ~(1<<5);           
	 i2c_hdmi_rx_wb(0x0C, md);// reg0c[5]=0 power down off
	 //------------------------------------------------------ 	 
	 
//     //(IO)(0x0C)[2]=0;power down;pown down off chip operation 
//	 //------------------------------------------------------            
//	 md = i2c_hdmi_rx_rb(0x0C);
//	 md |= (1<<2);           
//	 i2c_hdmi_rx_wb(0x0C, md);// reg0c[2]=1 cp power down
//	 //------------------------------------------------------ 		 
	 
	 my_delay(10);
	 
     data = i2c_hdmi_rx_rb(0x1B);//for debug
     data = i2c_hdmi_rx_r(0xEA);//for debug
     data = i2c_hdmi_rx_r(0xEB);//for debug
     data = i2c_hdmi_rx_rb(0xEA);//for debug
     data = i2c_hdmi_rx_rb(0xEB);//for debug
	 my_delay(100);
	 
	 //Set Slave Address Map
	 //------------------------------------------------------	 
     i2c_hdmi_rx_wb(0xFD, 0x46);//set cp map	address 
     i2c_hdmi_rx_wb(0xFB, 0x6A);//set hdmi map address 	 
     i2c_hdmi_rx_wb(0xF9, 0x66);//set KSV map address (repeater)
     i2c_hdmi_rx_wb(0xFA, 0x6E);//set EDID map address 
     i2c_hdmi_rx_wb(0xF8, 0x4E);//set DPLL(AFE) map address      
	 //------------------------------------------------------	 
	 my_delay(100);
     data =  i2c_hdmi_rx_rb(0xFD);//for debug 
     data =  i2c_hdmi_rx_rb(0xFB);//
     data =  i2c_hdmi_rx_rb(0xF9);//
     data =  i2c_hdmi_rx_rb(0xFA);// 
     data =  i2c_hdmi_rx_rb(0xF8);//     

//     //shut down hdmi section HPD self reset
//     md = i2c_hdmi_rx_hdmi_map_r(0x48);
//	 md |= (1<<6);            
//	 i2c_hdmi_rx_hdmi_map_w(0x48, md);//	
     
     //ADI Recommended Initialization Settings
	 //------------------------------------------------------     
     i2c_hdmi_rx_cp_map_wb(0x6C,0x00);//ADI recommended setting  
     data = i2c_hdmi_rx_cp_map_rb(0x6C);//ADI recommended setting  
     
     i2c_hdmi_rx_hdmi_map_wb(0xC0, 0x03);//ADI recommended setting      
     data = i2c_hdmi_rx_hdmi_map_rb(0xC0);//ADI recommended setting

     i2c_hdmi_rx_hdmi_map_wb(0x03, 0x98);//ADI recommended setting 
     data = i2c_hdmi_rx_hdmi_map_rb(0x03);//ADI recommended setting
     
     i2c_hdmi_rx_hdmi_map_wb(0x10, 0xA5);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x10);//ADI recommended setting     
     
     i2c_hdmi_rx_hdmi_map_wb(0x45, 0x04);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x45);//ADI recommended setting      
     
     i2c_hdmi_rx_hdmi_map_wb(0x97, 0xC0);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x97);//ADI recommended setting      
         
     i2c_hdmi_rx_hdmi_map_wb(0x3D, 0x10);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x3D);//ADI recommended setting       
     
     i2c_hdmi_rx_hdmi_map_wb(0x3E, 0x69);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x3E);//ADI recommended setting  
     
     i2c_hdmi_rx_hdmi_map_wb(0x3F, 0x46);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x3F);//ADI recommended setting     
     
     i2c_hdmi_rx_hdmi_map_wb(0x4E, 0xFE);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x4E);//ADI recommended setting     
     
     i2c_hdmi_rx_hdmi_map_wb(0x4F, 0x08);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x4F);//ADI recommended setting      
     
     i2c_hdmi_rx_hdmi_map_wb(0x50, 0x00);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x50);//ADI recommended setting       
     
     i2c_hdmi_rx_hdmi_map_wb(0x57, 0xA3);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x57);//ADI recommended setting       
     
     i2c_hdmi_rx_hdmi_map_wb(0x58, 0x07);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x58);//ADI recommended setting     
     
     i2c_hdmi_rx_hdmi_map_wb(0x93, 0x03);//ADI recommended setting 
     data = i2c_hdmi_rx_hdmi_map_rb(0x93);//ADI recommended setting      
     
     i2c_hdmi_rx_hdmi_map_wb(0x5A, 0x80);//ADI recommended setting 
     data = i2c_hdmi_rx_hdmi_map_rb(0x5A);//ADI recommended setting      
     
     //TMDS freq above 27MHZ   
     i2c_hdmi_rx_hdmi_map_wb(0x85, 0x10);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x85);//ADI recommended setting 
          
     i2c_hdmi_rx_hdmi_map_wb(0x86, 0x9B);//ADI recommended setting
     data = i2c_hdmi_rx_hdmi_map_rb(0x86);//ADI recommended setting 
     
     i2c_hdmi_rx_hdmi_map_wb(0x9B, 0x03);//ADI recommended setting 
     data = i2c_hdmi_rx_hdmi_map_rb(0x9B);//ADI recommended setting                                                                      
	 //------------------------------------------------------ 
 	 
	 

     //(IO)(0x15)[3:1]=000;A control to tristate the pixel data on the pixel pins; [1]=0:Pixel bus active [2]=0:LLC pin active [3]=0:Sync output pins active
	 //------------------------------------------------------       
     md = i2c_hdmi_rx_rb(0x15);
	 md &= ~(7<<1);           
	 i2c_hdmi_rx_wb(0x15, md);// reg15[3:1]=000
	 //------------------------------------------------------ 
	 data = i2c_hdmi_rx_rb(0x15);
	 	 
 	 //(IO)(0x14);drive strength;
	 //------------------------------------------------------               
	 i2c_hdmi_rx_wb(0x14, 0x3F);//drive strength 
	 //------------------------------------------------------  
	 data = i2c_hdmi_rx_rb(0x14);

	 //HPA_AUTO_INT_EDID()
	 //------------------------------------------------------ 	 
     md = i2c_hdmi_rx_hdmi_map_rb(0x6C);
	 md &= ~(3<<1);           
	 i2c_hdmi_rx_hdmi_map_wb(0x6C, md);// reg6c[2:1]=00
	 //------------------------------------------------------ 	 
     
     
     //(HDMI)(0x00)[2:0]=000;HDMI_PORT_SELECT;Set HDMI Input Port A       
	 //------------------------------------------------------       
     i2c_hdmi_rx_hdmi_map_wb(0x00, 0x00);    
	 //------------------------------------------------------     
     data = i2c_hdmi_rx_hdmi_map_rb(0x00);//for debug
        
                       
     //(HDMI)(0x83)[1:0]=10;CLOCK_TERMA_DISABLE;Enable Termination Port A      
	 //------------------------------------------------------                         
     i2c_hdmi_rx_hdmi_map_wb(0x83, 0xFE);//
	 //------------------------------------------------------                 
     data = i2c_hdmi_rx_hdmi_map_rb(0x83);//for debug
                  
	 
	 //(IO)(0x01) [6:4]Vertical freq           [3:0]Prime mode
	 //------------------------------------------------------  
	 i2c_hdmi_rx_wb(0x01, 0x06);//     
	 //------------------------------------------------------  	 
	 data = i2c_hdmi_rx_rb(0x01);
	 
	 //(IO)(0x00) [5:0]VID_STD        
	 //------------------------------------------------------      
	 i2c_hdmi_rx_wb(0x00, 0x02);
	 //------------------------------------------------------
	 data = i2c_hdmi_rx_rb(0x00);	 

    //(CP)(0xBA):free run disable
	 //------------------------------------------------------     
     md = i2c_hdmi_rx_cp_map_rb(0xBA);
	 md &= ~(1<<0);       	 
     i2c_hdmi_rx_cp_map_wb(0xBA,md);
	 //------------------------------------------------------      
     data = i2c_hdmi_rx_cp_map_rb(0xBA);//for debug
	 
	 
	 
//	 //(HDMI)(0x6C) [0]=1 HPA_MANUAL;default 0xA2 
//	 //------------------------------------ 
//     i2c_hdmi_rx_hdmi_map_wb(0x6C,0xA3);
//	 //------------------------------------
//	 
//	 //(IO)(0x20) [7]:A HPA 5V [6]B HPA 5V [3]:tristate A [2]tristate B
//	 //------------------------------------ 
//     i2c_hdmi_rx_hdmi_map_wb(0x20,0x34);
//	 //------------------------------------	 


//	 //Low Frequency Formats ?  1080P30 should be set ,but i am not sure 4K 30P should be set
//	 //------------------------------------------------------ 	
//     md = i2c_hdmi_rx_hdmi_map_rb(0x4C);
//	 md |= (1<<2);
//	 i2c_hdmi_rx_hdmi_map_wb(0x4C, md);// reg4C[2]=1
//	 //------------------------------------------------------ 	  	 
	  

     //4k2k
     //ADI Recommended 4k2k Settings
	 //------------------------------------------------------      
     i2c_hdmi_rx_hdmi_map_wb(0x1B,0x00);//do not reset FIFO by video PLL lock
     my_delay(10);
     data = i2c_hdmi_rx_hdmi_map_rb(0x1B);//for debug       
    
    i2c_hdmi_rx_dpll_map_wb(0xC3,0x80); 
    data = i2c_hdmi_rx_dpll_map_rb(0xC3);//for debug       
    
    i2c_hdmi_rx_dpll_map_wb(0xCF,0x03); 
    data = i2c_hdmi_rx_dpll_map_rb(0xCF);//for debug     
    
    i2c_hdmi_rx_dpll_map_wb(0xC3,0x80); 
    data = i2c_hdmi_rx_dpll_map_rb(0xC3);//for debug     
    
    i2c_hdmi_rx_wb(0xDD,0xA0);    
    data = i2c_hdmi_rx_rb(0xDD);	 
	    
	//CP bypass      
    md = i2c_hdmi_rx_rb(0xBF);
    md &= ~(1<<0);
    i2c_hdmi_rx_wb(0xBF, md);// regBF[0]=0   
    data = i2c_hdmi_rx_rb(0xBF);	  
    
//	//CP complete bypass      
//    md = i2c_hdmi_rx_rb(0xBF);
//    md |= (1<<0);
//    i2c_hdmi_rx_wb(0xBF, md);// regBF[0]=1   
//    data = i2c_hdmi_rx_rb(0xBF);	    
//	 //------------------------------------------------------   
//	 
//	 //444 passthough
//	 //------------------------------------------------------ 	 
//	 //UP_CONVERSION_MODE
//     md = i2c_hdmi_rx_hdmi_map_rb(0x1D);
//	 md |= (1<<5);
//	 i2c_hdmi_rx_hdmi_map_wb(0x1D, md);// reg1D[5]=1 
//	  
//	 //DS_WITHOUT_FILTER
//     md = i2c_hdmi_rx_rb(0xE0);
//	 md |= (1<<7);
//	 i2c_hdmi_rx_wb(0xE0, md);// regE0[7]=1 
	 
	 //output format
    if ( 1 )//RGB 444
    {
        i2c_hdmi_rx_wb(0x03, 0x54);//0x94 :2x16-bit ITU-656 SDR mode //0x54 2 x SDR 4:4:4 Interleaved
    }
    else                                                   //YUV422
    {
        i2c_hdmi_rx_wb(0x03, 0x94);//0x94 :2x16-bit ITU-656 SDR mode //0x54 2 x SDR 4:4:4 Interleaved
    }
	 
	 data = i2c_hdmi_rx_rb(0x03);		 	 
	 //------------------------------------------------------ 
	 	 

//	 //422 passthough
//	 //------------------------------------------------------
//	 //DPP_BYPASS_EN
//     md = i2c_hdmi_rx_cp_map_rb(0xBD);
//	 md |= (1<<4);       	 
//     i2c_hdmi_rx_cp_map_wb(0xBD,md);//reg0xBD[4]=1
//	 
//	  	 
//	 //UP_CONVERSION_MODE
//     md = i2c_hdmi_rx_hdmi_map_rb(0x1D);
//	 md &= ~(1<<5);
//	 i2c_hdmi_rx_hdmi_map_wb(0x1D, md);// reg1D[5]=0 
//	  
//	 //DS_WITHOUT_FILTER
//     md = i2c_hdmi_rx_rb(0xE0);
//	 md &= ~(1<<7);
//	 i2c_hdmi_rx_wb(0xE0, md);// regE0[7]=0 	
//	 
//	 //output format
//	 i2c_hdmi_rx_wb(0x03, 0x94);//0x94 :2x16-bit ITU-656 SDR mode //0x54 2 x SDR 4:4:4 Interleaved
//	 data = i2c_hdmi_rx_rb(0x03);	  
//	 //------------------------------------------------------ 	         
    
//========================================old adv7611 setting===============
//     i2c_hdmi_rx_hdmi_map_wb(0x8D, 0x04);//LFG                                   
//     i2c_hdmi_rx_hdmi_map_wb(0x8E, 0x1E);//HFG
                              
     //i2c_hdmi_rx_w(0x05, 0x28);//AV Codes Off
     i2c_hdmi_rx_wb(0x06, 0xA6);//Invert VS,HS pins    
//     i2c_hdmi_rx_wb(0x0B, 0x44);//Power up part 
//     i2c_hdmi_rx_wb(0x0C, 0x42);//Power up part     
     i2c_hdmi_rx_wb(0x14, 0x7F);//Max Drive Strength
//     i2c_hdmi_rx_wb(0x15, 0x80);//Disable Tristate of Pins
//     i2c_hdmi_rx_w(0x19, 0x83);//LLC DLL phase
//     i2c_hdmi_rx_w(0x33, 0x40);//LLC DLL enable
//     i2c_hdmi_rx_cp_map_wb(0xBA, 0x01);//Set HDMI FreeRun
                              
//     i2c_hdmi_rx_ksv_map_wb(0x40, 0x81);//Disable HDCP 1.1 features
//     i2c_hdmi_rx_hdmi_map_wb(0x00, 0x00);//Set HDMI Input Port A                 
//     i2c_hdmi_rx_hdmi_map_wb(0x83, 0xFE);//Enable clock terminator for port A    
//     i2c_hdmi_rx_hdmi_map_wb(0x6F, 0x0C);//ADI recommended setting               
//     i2c_hdmi_rx_hdmi_map_wb(0x85, 0x1F);//ADI recommended setting               
//     i2c_hdmi_rx_hdmi_map_wb(0x87, 0x70);//ADI recommended setting               
//     
//     i2c_hdmi_rx_hdmi_map_wb(0x1A, 0x8A);//unmute audio                          
//     i2c_hdmi_rx_hdmi_map_wb(0x57, 0xDA);//ADI recommended setting               
//     i2c_hdmi_rx_hdmi_map_wb(0x58, 0x01);//ADI recommended setting               
//     i2c_hdmi_rx_hdmi_map_wb(0x75, 0x10);//DDC drive strength      
//     
//     
//
//     i2c_hdmi_rx_cp_map_wb(0x6C, 0x00);  
//     i2c_hdmi_rx_hdmi_map_wb(0x6F, 0x0C);
//     i2c_hdmi_rx_hdmi_map_wb(0x85, 0x1F);
//     i2c_hdmi_rx_hdmi_map_wb(0x87, 0x70);
//     i2c_hdmi_rx_hdmi_map_wb(0x57, 0xDA);
//     i2c_hdmi_rx_hdmi_map_wb(0x58, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0x03, 0x98);
//     i2c_hdmi_rx_hdmi_map_wb(0x4C, 0x44);
//                                        
//     i2c_hdmi_rx_hdmi_map_wb(0xC1, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC2, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC3, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC4, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC5, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC6, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC7, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC8, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xC9, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xCA, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xCB, 0x01);
//     i2c_hdmi_rx_hdmi_map_wb(0xCC, 0x01);
//==========================================================
	 
 	 //4k2kEDID
	 //------------------------------------------------------  
	 //EDID disable
     md = i2c_hdmi_rx_ksv_map_rb(0x74);
	 md &= ~(1<<0);
	 i2c_hdmi_rx_ksv_map_wb(0x74, md);// reg74[0]=0      
	 
    md = i2c_hdmi_rx_hdmi_map_rb(0x5A);
    md |= (1<<3); 
    i2c_hdmi_rx_hdmi_map_wb(0x5A,md);// 0x5A[3]=1 reset EDID repeater
    md = i2c_hdmi_rx_hdmi_map_rb(0x5A);                             
    md &= ~(1<<3);                                                  
    i2c_hdmi_rx_hdmi_map_wb(0x5A,md);// 0x5A[3]=0 EDID start
 
// E-EDID 4k ROM
  i2c_hdmi_rx_edid_map_wb ( 0x00, 0x00 );//0
  i2c_hdmi_rx_edid_map_wb ( 0x01, 0xFF );//1
  i2c_hdmi_rx_edid_map_wb ( 0x02, 0xFF );//2
  i2c_hdmi_rx_edid_map_wb ( 0x03, 0xFF );//3
  i2c_hdmi_rx_edid_map_wb ( 0x04, 0xFF );//4
  i2c_hdmi_rx_edid_map_wb ( 0x05, 0xFF );//5
  i2c_hdmi_rx_edid_map_wb ( 0x06, 0xFF );//6
  i2c_hdmi_rx_edid_map_wb ( 0x07, 0x00 );//7
  i2c_hdmi_rx_edid_map_wb ( 0x08, 0x44 );//8
  i2c_hdmi_rx_edid_map_wb ( 0x09, 0x89 );//9
  i2c_hdmi_rx_edid_map_wb ( 0x0A, 0xD4 );//10
  i2c_hdmi_rx_edid_map_wb ( 0x0B, 0x03 );//11
  i2c_hdmi_rx_edid_map_wb ( 0x0C, 0x15 );//12
  i2c_hdmi_rx_edid_map_wb ( 0x0D, 0xCD );//13
  i2c_hdmi_rx_edid_map_wb ( 0x0E, 0x5B );//14
  i2c_hdmi_rx_edid_map_wb ( 0x0F, 0x07 );//15
  i2c_hdmi_rx_edid_map_wb ( 0x10, 0x15 );//16
  i2c_hdmi_rx_edid_map_wb ( 0x11, 0x15 );//17
  i2c_hdmi_rx_edid_map_wb ( 0x12, 0x01 );//18
  i2c_hdmi_rx_edid_map_wb ( 0x13, 0x03 );//19
  i2c_hdmi_rx_edid_map_wb ( 0x14, 0xA2 );//20//was 80
  i2c_hdmi_rx_edid_map_wb ( 0x15, 0x50 );//21
  i2c_hdmi_rx_edid_map_wb ( 0x16, 0x2D );//22
  i2c_hdmi_rx_edid_map_wb ( 0x17, 0x78 );//23
  i2c_hdmi_rx_edid_map_wb ( 0x18, 0x12 );//24//was 0x0A
  i2c_hdmi_rx_edid_map_wb ( 0x19, 0x0D );//25
  i2c_hdmi_rx_edid_map_wb ( 0x1A, 0xC9 );//26
  i2c_hdmi_rx_edid_map_wb ( 0x1B, 0xA0 );//27
  i2c_hdmi_rx_edid_map_wb ( 0x1C, 0x57 );//28
  i2c_hdmi_rx_edid_map_wb ( 0x1D, 0x47 );//29
  i2c_hdmi_rx_edid_map_wb ( 0x1E, 0x98 );//30
  i2c_hdmi_rx_edid_map_wb ( 0x1F, 0x27 );//31
  i2c_hdmi_rx_edid_map_wb ( 0x20, 0x12 );//32
  i2c_hdmi_rx_edid_map_wb ( 0x21, 0x48 );//33
  i2c_hdmi_rx_edid_map_wb ( 0x22, 0x4C );//34
  i2c_hdmi_rx_edid_map_wb ( 0x23, 0x20 );//35
  i2c_hdmi_rx_edid_map_wb ( 0x24, 0x00 );//36
  i2c_hdmi_rx_edid_map_wb ( 0x25, 0x00 );//37
  i2c_hdmi_rx_edid_map_wb ( 0x26, 0x01 );//38// TIMING I    (Hpixel/8)-31
  i2c_hdmi_rx_edid_map_wb ( 0x27, 0x01 );//39              MSB 2bit  1:1(00),4:3(01),5:4(10),16:9(11)  LSB 6bit  HZ-60
  i2c_hdmi_rx_edid_map_wb ( 0x28, 0x01 );//40// TIMING II
  i2c_hdmi_rx_edid_map_wb ( 0x29, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x2A, 0x01 );//  // TIMING III
  i2c_hdmi_rx_edid_map_wb ( 0x2B, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x2C, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x2D, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x2E, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x2F, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x30, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x31, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x32, 0x01 );//50
  i2c_hdmi_rx_edid_map_wb ( 0x33, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x34, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x35, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0x36, 0x02 );//54  BYTE 0 (detail timing 0)  Pixel Clock/10000                                                                                                                      0x02      0x01           
  i2c_hdmi_rx_edid_map_wb ( 0x37, 0x3A );//    BYTE 1                    Byte0/Byte1 LSB/HSB      74.25MHZ                                                                                                      0x3A      0x1D           
  i2c_hdmi_rx_edid_map_wb ( 0x38, 0x80 );//    BYTE 2                    H-Display low 8 bit                                                                                                                    0x80      0x80           
  i2c_hdmi_rx_edid_map_wb ( 0x39, 0x18 );//    BYTE 3                    H-Blanking low 8 bit                                                                                                                   0x18      0x3E           
  i2c_hdmi_rx_edid_map_wb ( 0x3A, 0x71 );//    BYTE 4                    H-D high 4 H-B high 4                    H-D 780h 1920d  H-B  33Eh 830d                                                                0x71      0x73           
  i2c_hdmi_rx_edid_map_wb ( 0x3B, 0x38 );//    BYTE 5                    V-Display low 8 bit                                                                                                                    0x38      0x38           
  i2c_hdmi_rx_edid_map_wb ( 0x3C, 0x2D );//60  BYTE 6                    V-Blanking low 8 bit                                                                                                                   0x2D      0x2D           
  i2c_hdmi_rx_edid_map_wb ( 0x3D, 0x40 );//    BYTE 7                    V-D high 4 V-B high 4                    V-D 438h 1080d  V-B  2Dh 45d                                                                  0x40      0x40           
  i2c_hdmi_rx_edid_map_wb ( 0x3E, 0x58 );//    BYTE 8   HSO 27E          Horizontal Sync Offset low 8              88                                                                                           0x58      0x58           
  i2c_hdmi_rx_edid_map_wb ( 0x3F, 0x2C );//    BYTE 9   HSPW 2C          Horizontal Sync Pulse Width low 8         44                                                                                           0x2C      0x2C           
  i2c_hdmi_rx_edid_map_wb ( 0x40, 0x45 );//    BYTE 10  VSO 4  VSW 5                 （Vertical Sync Offset low 4 , Sync Width low 4）  4 5                                                                     0x45      0x45           
  i2c_hdmi_rx_edid_map_wb ( 0x41, 0x00 );//    BYTE 11                   Horizontal Sync Offset high 2 ;Horizontal Sync Pulse Width high 2 ;Vertical Sync Offset high 2;Sync Width high 2                       0x00      0x00                       
  i2c_hdmi_rx_edid_map_wb ( 0x42, 0x20 );//    BYTE 12                  H low 8               09                                                                                                                0x09      0x09           
  i2c_hdmi_rx_edid_map_wb ( 0x43, 0xC2 );//    BYTE 13                  V low 8               37                                                                                                                0x25      0x25           
  i2c_hdmi_rx_edid_map_wb ( 0x44, 0x31 );//    BYTE 14                  H high 4 V high 4     2 1                                                                                                               0x21      0x21           
  i2c_hdmi_rx_edid_map_wb ( 0x45, 0x00 );//    BYTE 15                                                                                                                                                          0x00      0x00           
  i2c_hdmi_rx_edid_map_wb ( 0x46, 0x00 );//70  BYTE 16                                                                                                                                                          0x00      0x00           
  i2c_hdmi_rx_edid_map_wb ( 0x47, 0x1E );//    BYTE 17      00010011                                                                                                                                            0x1E      0x13                  
  i2c_hdmi_rx_edid_map_wb ( 0x48, 0x01);//    BYTE 0                                                            
  i2c_hdmi_rx_edid_map_wb ( 0x49, 0x1D);//    BYTE 1                                                            
  i2c_hdmi_rx_edid_map_wb ( 0x4A, 0x80);//    BYTE 2
  i2c_hdmi_rx_edid_map_wb ( 0x4B, 0x18);//75  BYTE 3
  i2c_hdmi_rx_edid_map_wb ( 0x4C, 0x71);//    BYTE 4
  i2c_hdmi_rx_edid_map_wb ( 0x4D, 0x1C);//    BYTE 5
  i2c_hdmi_rx_edid_map_wb ( 0x4E, 0x16);//    BYTE 6
  i2c_hdmi_rx_edid_map_wb ( 0x4F, 0x20);//    BYTE 7
  i2c_hdmi_rx_edid_map_wb ( 0x50, 0x58);//80  BYTE 8
  i2c_hdmi_rx_edid_map_wb ( 0x51, 0x2C);//    BYTE 9
  i2c_hdmi_rx_edid_map_wb ( 0x52, 0x25);//    BYTE 10
  i2c_hdmi_rx_edid_map_wb ( 0x53, 0x00);//    BYTE 11
  i2c_hdmi_rx_edid_map_wb ( 0x54, 0xC4);//    BYTE 12
  i2c_hdmi_rx_edid_map_wb ( 0x55, 0x8E);//    BYTE 13
  i2c_hdmi_rx_edid_map_wb ( 0x56, 0x21);//    BYTE 14
  i2c_hdmi_rx_edid_map_wb ( 0x57, 0x00);//    BYTE 15
  i2c_hdmi_rx_edid_map_wb ( 0x58, 0x00);//    BYTE 16
  i2c_hdmi_rx_edid_map_wb ( 0x59, 0x9E);//
  i2c_hdmi_rx_edid_map_wb ( 0x5A, 0x00 );//90 (detail timing 2)
  i2c_hdmi_rx_edid_map_wb ( 0x5B, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0x5C, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0x5D, 0xFC );//
  i2c_hdmi_rx_edid_map_wb ( 0x5E, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0x5F, 0x48 );//
  i2c_hdmi_rx_edid_map_wb ( 0x60, 0x44 );//
  i2c_hdmi_rx_edid_map_wb ( 0x61, 0x4D );//
  i2c_hdmi_rx_edid_map_wb ( 0x62, 0x49 );//
  i2c_hdmi_rx_edid_map_wb ( 0x63, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0x64, 0x41 );//100
  i2c_hdmi_rx_edid_map_wb ( 0x65, 0x6E );//
  i2c_hdmi_rx_edid_map_wb ( 0x66, 0x61 );//
  i2c_hdmi_rx_edid_map_wb ( 0x67, 0x6C );//
  i2c_hdmi_rx_edid_map_wb ( 0x68, 0x79 );//
  i2c_hdmi_rx_edid_map_wb ( 0x69, 0x7A );//
  i2c_hdmi_rx_edid_map_wb ( 0x6A, 0x65 );//
  i2c_hdmi_rx_edid_map_wb ( 0x6B, 0x72 );//
  i2c_hdmi_rx_edid_map_wb ( 0x6C, 0x00 );//  (detail timing 3)
  i2c_hdmi_rx_edid_map_wb ( 0x6D, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0x6E, 0x00 );//110
  i2c_hdmi_rx_edid_map_wb ( 0x6F, 0xFD );//
  i2c_hdmi_rx_edid_map_wb ( 0x70, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0x71, 0x17 );//              0x38
  i2c_hdmi_rx_edid_map_wb ( 0x72, 0xF1 );//              0x4C
  i2c_hdmi_rx_edid_map_wb ( 0x73, 0x08 );//              0x1E
  i2c_hdmi_rx_edid_map_wb ( 0x74, 0x8C );//              0x53
  i2c_hdmi_rx_edid_map_wb ( 0x75, 0x1E );//              0x11
  i2c_hdmi_rx_edid_map_wb ( 0x76, 0x00 );//              0x00
  i2c_hdmi_rx_edid_map_wb ( 0x77, 0x0A );//
  i2c_hdmi_rx_edid_map_wb ( 0x78, 0x20 );//120
  i2c_hdmi_rx_edid_map_wb ( 0x79, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0x7A, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0x7B, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0x7C, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0x7D, 0x20 );//       
  i2c_hdmi_rx_edid_map_wb ( 0x7E, 0x01 );//  
  i2c_hdmi_rx_edid_map_wb ( 0x7F, 0x81 );// initial was AB
  
  i2c_hdmi_rx_edid_map_wb ( 0x80, 0x02 );//0
  i2c_hdmi_rx_edid_map_wb ( 0x81, 0x03 );//1
  i2c_hdmi_rx_edid_map_wb ( 0x82, 0x77 );//2
  i2c_hdmi_rx_edid_map_wb ( 0x83, 0x71 );//3 was 71//51
  i2c_hdmi_rx_edid_map_wb ( 0x84, 0x5F );//4
  i2c_hdmi_rx_edid_map_wb ( 0x85, 0x90 );//5
  i2c_hdmi_rx_edid_map_wb ( 0x86, 0x1F );//6
  i2c_hdmi_rx_edid_map_wb ( 0x87, 0x22 );//7
  i2c_hdmi_rx_edid_map_wb ( 0x88, 0x20 );//8
  i2c_hdmi_rx_edid_map_wb ( 0x89, 0x05 );//9
  i2c_hdmi_rx_edid_map_wb ( 0x8A, 0x14 );//10
  i2c_hdmi_rx_edid_map_wb ( 0x8B, 0x04 );//11
  i2c_hdmi_rx_edid_map_wb ( 0x8C, 0x13 );//12
  i2c_hdmi_rx_edid_map_wb ( 0x8D, 0x3E );//13
  i2c_hdmi_rx_edid_map_wb ( 0x8E, 0x3C );//14
  i2c_hdmi_rx_edid_map_wb ( 0x8F, 0x11 );//15
  i2c_hdmi_rx_edid_map_wb ( 0x90, 0x02 );//16
  i2c_hdmi_rx_edid_map_wb ( 0x91, 0x03 );//17
  i2c_hdmi_rx_edid_map_wb ( 0x92, 0x15 );//18
  i2c_hdmi_rx_edid_map_wb ( 0x93, 0x06 );//19
  i2c_hdmi_rx_edid_map_wb ( 0x94, 0x01 );//20
  i2c_hdmi_rx_edid_map_wb ( 0x95, 0x07 );//21
  i2c_hdmi_rx_edid_map_wb ( 0x96, 0x08 );//22
  i2c_hdmi_rx_edid_map_wb ( 0x97, 0x09 );//23
  i2c_hdmi_rx_edid_map_wb ( 0x98, 0x0A );//24
  i2c_hdmi_rx_edid_map_wb ( 0x99, 0x0B );//25
  i2c_hdmi_rx_edid_map_wb ( 0x9A, 0x0C );//26//clock
  i2c_hdmi_rx_edid_map_wb ( 0x9B, 0x0D );//27
  i2c_hdmi_rx_edid_map_wb ( 0x9C, 0x0E );//28//
  i2c_hdmi_rx_edid_map_wb ( 0x9D, 0x0F );//29
  i2c_hdmi_rx_edid_map_wb ( 0x9E, 0x12 );//30
  i2c_hdmi_rx_edid_map_wb ( 0x9F, 0x16 );//31
  i2c_hdmi_rx_edid_map_wb ( 0xA0, 0x17 );//32
  i2c_hdmi_rx_edid_map_wb ( 0xA1, 0x18 );//33
  i2c_hdmi_rx_edid_map_wb ( 0xA2, 0x19 );//34
  i2c_hdmi_rx_edid_map_wb ( 0xA3, 0x1A );//35
  i2c_hdmi_rx_edid_map_wb ( 0xA4, 0x5F );//36
  i2c_hdmi_rx_edid_map_wb ( 0xA5, 0x1B );//37
  i2c_hdmi_rx_edid_map_wb ( 0xA6, 0x1C );//38
  i2c_hdmi_rx_edid_map_wb ( 0xA7, 0x1D );//39
  i2c_hdmi_rx_edid_map_wb ( 0xA8, 0x1E );//40
  i2c_hdmi_rx_edid_map_wb ( 0xA9, 0x21 );//
  i2c_hdmi_rx_edid_map_wb ( 0xAA, 0x23 );//  
  i2c_hdmi_rx_edid_map_wb ( 0xAB, 0x24 );//
  i2c_hdmi_rx_edid_map_wb ( 0xAC, 0x25 );////clock
  i2c_hdmi_rx_edid_map_wb ( 0xAD, 0x26 );//
  i2c_hdmi_rx_edid_map_wb ( 0xAE, 0x27 );//
  i2c_hdmi_rx_edid_map_wb ( 0xAF, 0x28 );//
  i2c_hdmi_rx_edid_map_wb ( 0xB0, 0x29 );//
  i2c_hdmi_rx_edid_map_wb ( 0xB1, 0x2A );//
  i2c_hdmi_rx_edid_map_wb ( 0xB2, 0x2B );//50
  i2c_hdmi_rx_edid_map_wb ( 0xB3, 0x2C );//
  i2c_hdmi_rx_edid_map_wb ( 0xB4, 0x2D );//
  i2c_hdmi_rx_edid_map_wb ( 0xB5, 0x2E );//
  i2c_hdmi_rx_edid_map_wb ( 0xB6, 0x2F );//54  
  i2c_hdmi_rx_edid_map_wb ( 0xB7, 0x30 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xB8, 0x31 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xB9, 0x32 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xBA, 0x33 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xBB, 0x34 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xBC, 0x35 );//60  
  i2c_hdmi_rx_edid_map_wb ( 0xBD, 0x36 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xBE, 0x37 );//clock    
  i2c_hdmi_rx_edid_map_wb ( 0xBF, 0x38 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC0, 0x39 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC1, 0x3A );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC2, 0x3B );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC3, 0x3D );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC4, 0x42 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC5, 0x3F );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC6, 0x40 );//70  
  i2c_hdmi_rx_edid_map_wb ( 0xC7, 0x32 );//    
  i2c_hdmi_rx_edid_map_wb ( 0xC8, 0x0F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xC9, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xCA, 0x07);//    
  i2c_hdmi_rx_edid_map_wb ( 0xCB, 0x17);//75  
  i2c_hdmi_rx_edid_map_wb ( 0xCC, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xCD, 0x50);//    
  i2c_hdmi_rx_edid_map_wb ( 0xCE, 0x3F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xCF, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD0, 0xC0);//80 //clock 
  i2c_hdmi_rx_edid_map_wb ( 0xD1, 0x57);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD2, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD3, 0x00);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD4, 0x5F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD5, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD6, 0x01);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD7, 0x67);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD8, 0x7F);//    
  i2c_hdmi_rx_edid_map_wb ( 0xD9, 0x00);//
  i2c_hdmi_rx_edid_map_wb ( 0xDA, 0x83 );//90 
  i2c_hdmi_rx_edid_map_wb ( 0xDB, 0x4F );//
  i2c_hdmi_rx_edid_map_wb ( 0xDC, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xDD, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xDE, 0x78 );//
  i2c_hdmi_rx_edid_map_wb ( 0xDF, 0x03 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE0, 0x0C );//
  i2c_hdmi_rx_edid_map_wb ( 0xE1, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE2, 0x10 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE3, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE4, 0x38 );//100
  i2c_hdmi_rx_edid_map_wb ( 0xE5, 0x3C );//
  i2c_hdmi_rx_edid_map_wb ( 0xE6, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE7, 0xA0 );//
  i2c_hdmi_rx_edid_map_wb ( 0xE8, 0x8A );//
  i2c_hdmi_rx_edid_map_wb ( 0xE9, 0x01 );//
  i2c_hdmi_rx_edid_map_wb ( 0xEA, 0x02 );//
  i2c_hdmi_rx_edid_map_wb ( 0xEB, 0x03 );//
  i2c_hdmi_rx_edid_map_wb ( 0xEC, 0x04 );//   
  i2c_hdmi_rx_edid_map_wb ( 0xED, 0x81 );//
  i2c_hdmi_rx_edid_map_wb ( 0xEE, 0x40 );//110
  i2c_hdmi_rx_edid_map_wb ( 0xEF, 0x20 );//
  i2c_hdmi_rx_edid_map_wb ( 0xF0, 0x30 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF1, 0x40 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF2, 0x50 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF3, 0x60 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF4, 0x70 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF5, 0x80 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF6, 0x90 );//                          
  i2c_hdmi_rx_edid_map_wb ( 0xF7, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xF8, 0x00 );//120
  i2c_hdmi_rx_edid_map_wb ( 0xF9, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xFA, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xFB, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xFC, 0x00 );//
  i2c_hdmi_rx_edid_map_wb ( 0xFD, 0x00 );//       
  i2c_hdmi_rx_edid_map_wb ( 0xFE, 0x00 );//  
  i2c_hdmi_rx_edid_map_wb ( 0xFF, 0xFB );//was FB //1D
  data = i2c_hdmi_rx_edid_map_rb (0x7F);
 
 
	 //EDID A enable
	 //------------------------------------
     md = i2c_hdmi_rx_ksv_map_rb(0x74);
	 md |= (1<<0);
	 i2c_hdmi_rx_ksv_map_wb(0x74, md);// reg74[0]=1
	 //------------------------------------
	 
	 
	 
//	 //(IO)(0x20) [7]:A HPA 5V [6]B HPA 5V [3]:tristate A [2]tristate B
//	 //------------------------------------ 
//     i2c_hdmi_rx_hdmi_map_wb(0x20,0xB4);
//	 //------------------------------------	 	 
	 
     #if 0
	 my_delay(500);
	 *(volatile unsigned int *)RESET_BASE = 0x05;
     #endif
	 
	 
	 
	 
	 
	 
	 
	 
////////////////////for phy 0 debug below/////////////////////////////////

//        data = 0x7A;
//        data = 0x7A;
  /*      data = 0x7A;
        data = 0x7A; 
        
      data =  i2c_hdmi_rx_hdmi_map_rb(0x52);//7d
      data =  i2c_hdmi_rx_hdmi_map_rb(0x51);//94   
      
     data =  i2c_hdmi_rx_hdmi_map_rb(0x0B);//0x08 [7:6]=00 8 bit per channel     
     data =  i2c_hdmi_rx_hdmi_map_rb(0x0B);     
     
     data =  i2c_hdmi_rx_hdmi_map_rb(0x1C);//c     
     data =  i2c_hdmi_rx_hdmi_map_rb(0x1C);    
     
     data =  i2c_hdmi_rx_hdmi_map_rb(0x07); //af   
     data =  i2c_hdmi_rx_hdmi_map_rb(0x07); 
     
     data = i2c_hdmi_rx_rb(0x6A);   //53  
     data = i2c_hdmi_rx_rb(0x6A);    
     
     data = i2c_hdmi_rx_rb(0x03);                          
      
      data =  i2c_hdmi_rx_hdmi_map_rb(0x04); //23 
      
      data =  i2c_hdmi_rx_hdmi_map_rb(0x05);               
      data =  i2c_hdmi_rx_hdmi_map_rb(0x05);  //B0       
        
      data =  i2c_hdmi_rx_hdmi_map_rb(0x53);//0x01 [3:0]=0001=rgb_full
      data =  i2c_hdmi_rx_hdmi_map_rb(0x53);
      data =  i2c_hdmi_rx_hdmi_map_rb(0x1F);
      data =  i2c_hdmi_rx_hdmi_map_rb(0x1F);      */
      
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x07);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x07);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x08);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x08);     
//      
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x20);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x20);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x21);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x21);        
//      
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x22);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x22);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x23);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x23);     
//      
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x24);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x24);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x25);
//      data =  i2c_hdmi_rx_hdmi_map_rb(0x25);               
     
//     checka0 = i2c_hdmi_rx_rb(0x00);
//     checka1 = i2c_hdmi_rx_rb(0x01);
//     checka2 = i2c_hdmi_rx_hdmi_map_rb(0x83);
//   }
//
//	 /*******************************
//      PHY1
//    ********************************/   

      return(data);

}


