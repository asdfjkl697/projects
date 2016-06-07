/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
* main.c
shell-init: error retrieving current directory: getcwd: cannot access parent directories: No such file or directory
chdir: error retrieving current directory: getcwd: cannot access parent directories: No such file or directory
* Copyright (C) 2014 gsj0791 <gsj0791@163.com>
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>

const uint16_t wCRCTalbeAbs[]	=	{0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400, }; 
uint16_t CRC16_2(uint8_t* pchMsg, uint16_t wDataLen) 
{     
	uint16_t wCRC = 0xFFFF; uint16_t i;uint8_t chChar;         
	for (i = 0; i < wDataLen; i++)         
	{     
		chChar = *pchMsg++; 
    	wCRC = wCRCTalbeAbs[(chChar ^ wCRC) & 15] ^(wCRC>>4);                 
		wCRC = wCRCTalbeAbs[((chChar >> 4) ^ wCRC) & 15] ^ (wCRC >> 4); 
  	}        
	return wCRC; 
} 

