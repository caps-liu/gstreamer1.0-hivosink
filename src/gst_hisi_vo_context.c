/*
* Hisi VO Context
*/

#include <unistd.h>
#include <pthread.h>
#include <gst_hisi_vo_context.h>
#include <assert.h>

#define CONTEXT_OK (0)
#define CONTEXT_FAILED (-1)

#define DAC_CVBS         3
#define DAC_YPBPR_Y      1
#define DAC_YPBPR_PB     2
#define DAC_YPBPR_PR     0

static HI_HANDLE g_window_handle;


HI_S32 HIADP_Disp_Init(HI_UNF_ENC_FMT_E enFormat)
{
    HI_S32                      Ret;
    HI_UNF_DISP_BG_COLOR_S      BgColor;
    HI_UNF_DISP_INTF_S          stIntf[2];
    HI_UNF_DISP_OFFSET_S        offset;

    Ret = HI_UNF_DISP_Init();
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Init failed, Ret=%#x.\n", Ret);
        return Ret;
    }

    /* set display1 interface */
    stIntf[0].enIntfType                = HI_UNF_DISP_INTF_TYPE_YPBPR;
    stIntf[0].unIntf.stYPbPr.u8DacY     = DAC_YPBPR_Y;
    stIntf[0].unIntf.stYPbPr.u8DacPb    = DAC_YPBPR_PB;
    stIntf[0].unIntf.stYPbPr.u8DacPr    = DAC_YPBPR_PR;
    stIntf[1].enIntfType                = HI_UNF_DISP_INTF_TYPE_HDMI;
    stIntf[1].unIntf.enHdmi             = HI_UNF_HDMI_ID_0;
    Ret = HI_UNF_DISP_AttachIntf(HI_UNF_DISPLAY1, &stIntf[0], 2);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_AttachIntf failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_DeInit();
        return Ret;
    }   

    /* set display0 interface */
    stIntf[0].enIntfType            = HI_UNF_DISP_INTF_TYPE_CVBS;
    stIntf[0].unIntf.stCVBS.u8Dac   = DAC_CVBS;
    Ret = HI_UNF_DISP_AttachIntf(HI_UNF_DISPLAY0, &stIntf[0], 1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_AttachIntf failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_DeInit();
        return Ret;
    }  

    Ret = HI_UNF_DISP_Attach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Attach failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_DeInit();
        return Ret;        
    }

    /* set display1 format*/
    Ret = HI_UNF_DISP_SetFormat(HI_UNF_DISPLAY1, enFormat); 
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_SetFormat failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;          
    }

    /* set display0 format*/
    if ((HI_UNF_ENC_FMT_1080P_60 == enFormat)
        ||(HI_UNF_ENC_FMT_1080i_60 == enFormat)
        ||(HI_UNF_ENC_FMT_720P_60 == enFormat)
        ||(HI_UNF_ENC_FMT_480P_60 == enFormat)
        ||(HI_UNF_ENC_FMT_NTSC == enFormat))
    {
        Ret = HI_UNF_DISP_SetFormat(HI_UNF_DISPLAY0, HI_UNF_ENC_FMT_NTSC);
        if (HI_SUCCESS != Ret)
        {
            g_printf("call HI_UNF_DISP_SetFormat failed, Ret=%#x.\n", Ret);
            HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
            HI_UNF_DISP_DeInit();
            return Ret;
        }
    }
    
    if ((HI_UNF_ENC_FMT_1080P_50 == enFormat)
        ||(HI_UNF_ENC_FMT_1080i_50 == enFormat)
        ||(HI_UNF_ENC_FMT_720P_50 == enFormat)
        ||(HI_UNF_ENC_FMT_576P_50 == enFormat)
        ||(HI_UNF_ENC_FMT_PAL == enFormat))
    {
        Ret = HI_UNF_DISP_SetFormat(HI_UNF_DISPLAY0, HI_UNF_ENC_FMT_PAL);
        if (HI_SUCCESS != Ret)
        {
            g_printf("call HI_UNF_DISP_SetFormat failed, Ret=%#x.\n", Ret);
            HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
            HI_UNF_DISP_DeInit();
            return Ret;
        }
    }    

    Ret = HI_UNF_DISP_SetVirtualScreen(HI_UNF_DISPLAY1, 1920, 1080);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_SetVirtualScreen failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;          
    }

    offset.u32Left      = 0;
    offset.u32Top       = 0;
    offset.u32Right     = 0;
    offset.u32Bottom    = 0;   
    /*set display1 screen offset*/
    Ret = HI_UNF_DISP_SetScreenOffset(HI_UNF_DISPLAY1, &offset);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_SetBgColor failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;          
    }

    /*set display0 screen offset*/
    Ret = HI_UNF_DISP_SetScreenOffset(HI_UNF_DISPLAY0, &offset);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_SetBgColor failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;          
    }

    BgColor.u8Red   = 0;
    BgColor.u8Green = 0;
    BgColor.u8Blue  = 0;
    Ret = HI_UNF_DISP_SetBgColor(HI_UNF_DISPLAY1, &BgColor);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_SetBgColor failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;          
    }
    
    Ret = HI_UNF_DISP_Open(HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Open DISPLAY1 failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;
    }

    Ret = HI_UNF_DISP_Open(HI_UNF_DISPLAY0);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Open DISPLAY0 failed, Ret=%#x.\n", Ret);
        HI_UNF_DISP_Close(HI_UNF_DISPLAY1);
        HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
        HI_UNF_DISP_DeInit();
        return Ret;
    }
    
    return HI_SUCCESS;
}


HI_S32 HIADP_Disp_DeInit(HI_VOID)
{
    HI_S32                      Ret;

    Ret = HI_UNF_DISP_Close(HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Close failed, Ret=%#x.\n", Ret);
        return Ret;
    }

    Ret = HI_UNF_DISP_Close(HI_UNF_DISPLAY0);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Close failed, Ret=%#x.\n", Ret);
        return Ret;
    }

    Ret = HI_UNF_DISP_Detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_Detach failed, Ret=%#x.\n", Ret);
        return Ret;
    }

    Ret = HI_UNF_DISP_DeInit();
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_DISP_DeInit failed, Ret=%#x.\n", Ret);
        return Ret;
    }
        
    return HI_SUCCESS;
}

static HI_S32 HIADP_VO_DeInit()
{
    HI_S32         Ret;

#if 0
    Ret = HI_UNF_VO_Close(HI_UNF_DISPLAY1);
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_VO_Close failed.\n");
        return Ret;
    }
#endif
    Ret = HI_UNF_VO_DeInit();
    if (Ret != HI_SUCCESS)
    {
        g_printf("call HI_UNF_VO_DeInit failed.\n");
        return Ret;
    }

    return HI_SUCCESS;
}

static int hi_vo_init ()
{
  HI_UNF_WINDOW_ATTR_S WinAttr;
  HI_HANDLE hWin;
  int ret;

  ret = HI_UNF_VO_Init (HI_UNF_VO_DEV_MODE_NORMAL);
  if (HI_SUCCESS != ret) {
    g_printf ("call HI_UNF_VO_Init failed. ret:0x%x");
    return HI_FAILURE;
  }

  WinAttr.enDisp = HI_UNF_DISPLAY1;
  WinAttr.bVirtual = HI_FALSE;
  WinAttr.stWinAspectAttr.enAspectCvrs = HI_UNF_VO_ASPECT_CVRS_IGNORE;
  WinAttr.stWinAspectAttr.bUserDefAspectRatio = HI_FALSE;
  WinAttr.stWinAspectAttr.u32UserAspectWidth  = 0;
  WinAttr.stWinAspectAttr.u32UserAspectHeight = 0;
  WinAttr.bUseCropRect = HI_FALSE;
  WinAttr.stInputRect.s32X = 0;
  WinAttr.stInputRect.s32Y = 0;
  WinAttr.stInputRect.s32Width = 0;
  WinAttr.stInputRect.s32Height = 0;
  WinAttr.stOutputRect.s32X = 0;
  WinAttr.stOutputRect.s32Y = 0;
  WinAttr.stOutputRect.s32Width = 1080;
  WinAttr.stOutputRect.s32Height = 720;

  ret = HI_UNF_VO_CreateWindow (&WinAttr, &hWin);
  if (HI_SUCCESS != ret) {
    g_printf ("call HI_UNF_VO_CreateWindow failed. ret:0x%x", ret);
    goto vo_deinit;
  }

  ret = HI_UNF_VO_SetWindowEnable (hWin, HI_TRUE);
  if (HI_SUCCESS != ret) {
    g_printf ("call HI_UNF_VO_SetWindowEnable failed. ret:0x%x", ret);
    goto win_destroy;
  }

  g_window_handle = hWin;
  HI_UNF_VO_SetQuickOutputEnable(hWin, HI_TRUE);
  g_printf ("vo window created, vo_hdl:%p", hWin);

  return HI_SUCCESS;

win_destroy:
  if (HI_SUCCESS != HI_UNF_VO_DestroyWindow (hWin))
    g_printf ("call HI_UNF_VO_DestroyWindow failed.");

vo_deinit:
  if (HI_SUCCESS != HI_UNF_VO_DeInit ())
    g_printf ("call HI_UNF_VO_DeInit failed.");

  return HI_FAILURE;
}

static HI_VOID SetVideoFrameInfoDefaultValue(HI_UNF_VIDEO_FRAME_INFO_S *pstFrame,
                                HI_U32 u32W, HI_U32 u32H, HI_U32 u32PhyAddr)
{
    HI_U32 u32StrW;

    if (!pstFrame)
    {
        g_printf("Input null pointer!\n");
        return;
    }

    memset(pstFrame, 0, sizeof(HI_UNF_VIDEO_FRAME_INFO_S));

    u32StrW = (u32W + 0xf) & 0xFFFFFFF0ul;

    pstFrame->u32FrameIndex = 0;
    pstFrame->stVideoFrameAddr[0].u32YAddr = u32PhyAddr;
    pstFrame->stVideoFrameAddr[0].u32CAddr = u32PhyAddr + (u32StrW * u32H);
    pstFrame->stVideoFrameAddr[0].u32YStride = u32StrW;
    pstFrame->stVideoFrameAddr[0].u32CStride = u32StrW;

    pstFrame->u32Width  = u32W;
    pstFrame->u32Height = u32H;

    pstFrame->u32SrcPts = 0xffffffff;  /* 0xffffffff means unknown */
    pstFrame->u32Pts    = 0xffffffff;  /* 0xffffffff means unknown */

    pstFrame->u32AspectWidth  = 0;
    pstFrame->u32AspectHeight = 0;

    memset(&(pstFrame->stFrameRate), 0, sizeof(HI_UNF_VCODEC_FRMRATE_S));

    pstFrame->enVideoFormat = HI_UNF_FORMAT_YUV_SEMIPLANAR_420;
    pstFrame->bProgressive = HI_TRUE;
    pstFrame->enFieldMode  = HI_UNF_VIDEO_FIELD_ALL;
    pstFrame->bTopFieldFirst = HI_TRUE;

    pstFrame->enFramePackingType = HI_UNF_FRAME_PACKING_TYPE_NONE;
    pstFrame->u32Circumrotate = 0;
    pstFrame->bVerticalMirror = HI_FALSE;
    pstFrame->bHorizontalMirror = HI_FALSE;

    pstFrame->u32DisplayCenterX = pstFrame->u32Width/2;
    pstFrame->u32DisplayCenterY = pstFrame->u32Height/2;
    pstFrame->u32DisplayWidth  = pstFrame->u32Width;
    pstFrame->u32DisplayHeight = pstFrame->u32Height;

    pstFrame->u32ErrorLevel = 0;

    return;
}

static int reset_local_hisi_vo()
{
	HI_UNF_VO_ResetWindow(g_window_handle, HI_UNF_WINDOW_FREEZE_MODE_BLACK);
}

static int open_local_hisi_vo()
{
	HI_S32 Ret;
	Ret = hi_vo_init();

	if (Ret != HI_SUCCESS)
	{
		goto ERR;
	}

	g_printf("\e[31m open_local_hisi_vo :%s \e[0m\n", __func__);

	return CONTEXT_OK;

ERR:
	HI_UNF_VO_DestroyWindow(g_window_handle);
	HIADP_VO_DeInit();
	return CONTEXT_FAILED;
}

static int close_local_hisi_vo()
{
	HI_UNF_VO_SetWindowEnable(g_window_handle, HI_FALSE);
	HI_UNF_VO_DestroyWindow(g_window_handle);
	HIADP_VO_DeInit();

	g_printf("\e[31m close_local_hisi_vo :%s \e[0m\n", __func__);

	return CONTEXT_OK;
}

static int render_to_local_hisi_vo(int width, int height, HI_U32 addr)
{
	HI_S32 ret;
	HI_UNF_VIDEO_FRAME_INFO_S FrameInfo;
	
    SetVideoFrameInfoDefaultValue(&FrameInfo, width, height, (HI_U32)addr);
    ret = HI_UNF_VO_QueueFrame(g_window_handle, &FrameInfo);
    if (HI_SUCCESS != ret)
        g_printf ("put frame to VO failed 0x%x!", ret);
	
	return ret;
}

static HI_HANDLE local_get_window_handle()
{
    return g_window_handle;
}

static HisiVideoOutputContext g_vo_output_context = {
	.open  = open_local_hisi_vo,
	.close = close_local_hisi_vo,
	.reset = reset_local_hisi_vo,
	.render = render_to_local_hisi_vo,
	.get_window_handle = local_get_window_handle
};

HisiVideoOutputContext *hisi_vo_context_get()
{
	return &g_vo_output_context;
}


