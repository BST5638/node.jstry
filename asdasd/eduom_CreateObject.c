/******************************************************************************/
/*                                                                            */
/*    Copyright (c) 2013-2015, Kyu-Young Whang, KAIST                         */
/*    All rights reserved.                                                    */
/*                                                                            */
/*    Redistribution and use in source and binary forms, with or without      */
/*    modification, are permitted provided that the following conditions      */
/*    are met:                                                                */
/*                                                                            */
/*    1. Redistributions of source code must retain the above copyright       */
/*       notice, this list of conditions and the following disclaimer.        */
/*                                                                            */
/*    2. Redistributions in binary form must reproduce the above copyright    */
/*       notice, this list of conditions and the following disclaimer in      */
/*       the documentation and/or other materials provided with the           */
/*       distribution.                                                        */
/*                                                                            */
/*    3. Neither the name of the copyright holder nor the names of its        */
/*       contributors may be used to endorse or promote products derived      */
/*       from this software without specific prior written permission.        */
/*                                                                            */
/*    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     */
/*    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       */
/*    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS       */
/*    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE          */
/*    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,    */
/*    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;        */
/*    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER        */
/*    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT      */
/*    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN       */
/*    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE         */
/*    POSSIBILITY OF SUCH DAMAGE.                                             */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational Purpose Object Storage System            */
/*    (Version 1.0)                                                           */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: odysseus.educosmos@gmail.com                                    */
/*                                                                            */
/******************************************************************************/
/*
 * Module : eduom_CreateObject.c
 * 
 * Description :
 *  eduom_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */


#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"



/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
    ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
    ObjectID 	*nearObj,	/* IN create the new object near this object */
    ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
    Four	length,		/* IN amount of data */
    char	*data,		/* IN the initial data for the object */
    ObjectID	*oid)		/* OUT the object's ObjectID */
{
	/* These local variables are used in the solution code. However, you don’t have to use all these variables in your code, and you may also declare and use additional local variables if needed. */
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* pointer to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    

    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    /* Error check whether using not supported functionality by EduOM */
    if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
	alignedLen = MAX(sizeof(ShortPageID), ALIGNED_LENGTH(length));
	neededSpace = sizeof(ObjectHdr) + alignedLen + sizeof(SlottedPageSlot);//자유공간크기계산
	e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);//file id얻기
	if (e < 0) ERR(e);
	GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
	fid = catEntry->fid;
	eff = catEntry->eff;
	MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
	e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);
	if (e < 0) ERR(e);
	if (nearObj != NULL) {//인접한 오브젝트확인
		pid = *((PageID *)nearObj);//있으면 가져오기

	}
	else {
		e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
		if (e < 0) ERR(e);
		GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
		//필요공간에 맞는 avaiable list가 존재할경우
		if ((neededSpace <= SP_10SIZE) && (catEntry->availSpaceList10 >= 0))
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->availSpaceList10);
		else if ((neededSpace <= SP_20SIZE) && (catEntry->availSpaceList20 >= 0))
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->availSpaceList20);
		else if ((neededSpace <= SP_30SIZE) && (catEntry->availSpaceList30 >= 0))
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->availSpaceList30);
		else if ((neededSpace <= SP_40SIZE) && (catEntry->availSpaceList40 >= 0))
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->availSpaceList40);
		else if ((neededSpace <= SP_50SIZE) && (catEntry->availSpaceList50 >= 0))
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->availSpaceList50);
		else
			MAKE_PAGEID(pid, catEntry->fid.volNo, catEntry->lastPage);
		e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);
		if (e < 0) ERR(e);
	}
	e = BfM_GetTrain(&pid, (char **)&apage, PAGE_BUF);
	if (e < 0) ERR(e);
	needToAllocPage = FALSE;
	if (SP_FREE(apage) < neededSpace) {//페이지에 공간이 없을때 새로운 페이지받아오기
		e = BfM_FreeTrain(&pid, PAGE_BUF);
		if (e < 0) ERR(e);
		needToAllocPage = TRUE;
	}
	else {//아닐 경우 avaialbe space list에서 삭제
		e = om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
		if (e < 0) ERRB1(e, &pid, PAGE_BUF);
	}
	if (needToAllocPage) {
		e = RDsM_PageIdToExtNo((PageID *)&pFid, &firstExt);
		if (e < 0) ERR(e);
		if (nearObj != NULL) {
			nearPid = *((PageID *)nearObj);// 페이지할당
		}
		else {
			MAKE_PAGEID(nearPid, catEntry->fid.volNo, catEntry->lastPage);
		}
		e = RDsM_AllocTrains(fid.volNo, firstExt, &nearPid, eff, 1, PAGESIZE2, &pid);
		if (e < 0) ERR(e);
		e = BfM_GetNewTrain(&pid, (char **)&apage, PAGE_BUF);
		if (e < 0) ERR(e);
		apage->header.fid = fid; //헤더초기화
		apage->header.nSlots = 1;
		apage->header.free = 0;
		apage->header.unused = 0;
		apage->header.prevPage = NIL;
		apage->header.nextPage = NIL;
		apage->header.spaceListPrev = NIL;
		apage->header.spaceListNext = NIL;
		apage->header.unique = 0;
		apage->header.uniqueLimit = 0;
		apage->header.pid = pid;

		/* initialize */
		apage->slot[0].offset = EMPTYSLOT;

		e = om_FileMapAddPage(catObjForFile, (PageID *)nearObj, &pid);
		if (e < 0) ERRB1(e, &pid, PAGE_BUF);

	}
	else {
		if (SP_CFREE(apage) < neededSpace) {
			e = OM_CompactPage(apage, NIL);
			if (e < 0) ERRB1(e, &pid, PAGE_BUF);
		}
	}
	//선정된 페이지에 오브젝트 삽입
	obj = (Object *)&(apage->data[apage->header.free]);
	obj->header = *objHdr;//오브젝터 헤드복사
	obj->header.length = length;//길이갱신
	memcpy(obj->data, data, length);// 오브젝트 복사
	for (i = 0; i < apage->header.nSlots; i++)//slot 컨텐츠 업데이트
		if (apage->slot[-i].offset == EMPTYSLOT) break;
	if (i == apage->header.nSlots)//빈슬롯이 없을때 새로운 슬롯만들기
		apage->header.nSlots++;
	apage->slot[-i].offset = apage->header.free;
	e = om_GetUnique(&pid, &(apage->slot[-i].unique));
	if (e < 0) ERRB1(e, &pid, PAGE_BUF);
	apage->header.free += sizeof(ObjectHdr) + alignedLen;//free space 포인터 갱신
	if (oid != NULL)
		MAKE_OBJECTID(*oid, pid.volNo, pid.pageNo, i, apage->slot[-i].unique);//oid생성
	e = om_PutInAvailSpaceList(catObjForFile, &pid, apage);//page를 알맞은 avaiable list에 삽입
	if (e < 0) ERRB1(e, &pid, PAGE_BUF);
	e = BfM_SetDirty(&pid, PAGE_BUF);
	if (e < 0) ERRB1(e, &pid, PAGE_BUF);
	e = BfM_FreeTrain(&pid, PAGE_BUF);
	if (e < 0) ERR(e);
    return(eNOERROR);
    
} /* eduom_CreateObject() */
