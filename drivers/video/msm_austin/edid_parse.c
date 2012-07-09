
#include <linux/kernel.h>
#include <linux/string.h>
#include "edid_parse.h"




#define EDID_WORKAROUND 1

struct MyEdidInfo               cMyEdidInfo;
struct MyVendorProductId        cMyVendorProductId;
struct MyEdidVerRev             cMyEdidVerRev;
struct MyBasicDispParams        cMyBasicDispParams;
struct MyColorCharacteristics   cMyColorCharacteristics;
struct MyEstablishedTiming      cMyEstablishedTiming;
struct MyMonitorDescriptorBlock cMyMonitorDescriptorBlock;

struct MySpadPayload            cMySpadPayload;
struct MyCeaDataBlock           cMyCeaDataBlock;
struct MyColorimetryDataBlock   cMyColorimetryDataBlock;
struct MyCeaTimingExtension     cMyCeaTimingExtension;

HTX_INT segment_number[128];

HTX_INT st_count = 0;
HTX_INT dtd_count = 0;
HTX_INT sad_count = 0;
HTX_INT svd_count = 0;

static VendorProductId init_vendor_product_id( VendorProductId vpi );
static EdidVerRev init_edid_ver_rev( EdidVerRev evr );
static BasicDispParams init_basic_disp_params( BasicDispParams bdp );
static ColorCharacteristics init_color_characteristics( ColorCharacteristics cc );
static EstablishedTiming init_established_timing( EstablishedTiming et );
static MonitorDescriptorBlock init_monitor_descriptor_block( MonitorDescriptorBlock mdb );
static SpadPayload init_spad_payload( SpadPayload sp );
static ColorimetryDataBlock init_cdb( ColorimetryDataBlock cdb );
static CeaDataBlock init_cea_data_block( CeaDataBlock cea );
static CeaTimingExtension init_cea_timing_extension( CeaTimingExtension cea );



static void add_dtb( struct MyDetailedTimingBlockList dtb, EdidInfo e );
static void delete_dtbs( EdidInfo e );


static void add_sad( struct MySADescriptorList sad,EdidInfo e );
static void delete_sads( EdidInfo e );


static void add_svd( struct MySVDescriptorList svd,EdidInfo e );
static void delete_svds( EdidInfo e );


static void add_st( struct MyStandardTimingList st, EdidInfo e );

static void delete_sts( EdidInfo e );
static HTX_INT get_bits( HTX_BYTE in, HTX_INT high_bit, HTX_INT low_bit );

static HTX_BOOL parse_0_block( EdidInfo e, HTX_BYTE *d );
static HTX_BOOL parse_cea_block( EdidInfo e, HTX_BYTE *d, HTX_INT block_number );
static HTX_BOOL parse_block_map( HTX_BYTE *d, HTX_INT block_number );
struct MyDetailedTimingBlockList parse_dtb( HTX_INT i, HTX_BYTE *d );

HTX_INT segment_number_counter;

static HTX_BOOL parse_block_map( HTX_BYTE *d, HTX_INT block_number )
{
    HTX_INT i, array_size = 0;
    segment_number_counter = 0;

    for( i = block_number + 1; i < ( block_number + 0x80 ); i += 2 ) {
        if( ( d[i] == 0x02 ) || ( d[i+1] == 0x02 ) )
	    array_size++;
    }

    for( i = block_number + 1; i < ( block_number + 0x80 ); i += 2 ) {
        if( ( d[i] == 0x02 ) || ( d[i+1] == 0x02 ) ) {
	    segment_number[ segment_number_counter ] = ( ( ( i - block_number ) + 1 ) / 2 );
	    segment_number_counter++;
        }
    }

    segment_number[ segment_number_counter ] = 0;

    if( segment_number_counter != (array_size) )
        printk(KERN_ERR "Error: Problem parsing block map" );

    segment_number_counter = 0;

    return( HTX_FALSE );
}

struct MyDetailedTimingBlockList parse_dtb( HTX_INT i, HTX_BYTE *d )
{
    struct MyDetailedTimingBlockList tmp={0};
    
    tmp.pix_clk          = ( (HTX_WORD)d[i] | (HTX_WORD)( d[i+1] << 8 ) );
    tmp.h_active         = d[i+2] | ( get_bits( d[i+4],7,4 ) << 8 );
    tmp.h_blank          = d[i+3] | ( get_bits( d[i+4],3,0 ) << 8 );
    tmp.v_active         = d[i+5] | ( get_bits( d[i+7],7,4 ) << 8 );
    tmp.v_blank          = d[i+6] | ( get_bits( d[i+7],3,0 ) << 8 );	
    tmp.hsync_offset     = d[i+8] | ( get_bits( d[i+11],7,6 ) << 8 );
    tmp.hsync_pw         = d[i+9] | ( get_bits( d[i+11],5,4 ) << 8 );
    tmp.vsync_offset     = get_bits( d[i+10],8,4 ) | ( get_bits( d[i+11],3,2 ) << 8 );
    tmp.vsync_pw         = get_bits( d[i+10],3,0 ) | ( get_bits( d[i+11],0,1 ) << 8 );
    tmp.h_image_size     = d[i+12] | ( get_bits( d[i+14],7,4 ) << 8 );
    tmp.v_image_size     = d[i+13] | ( get_bits( d[i+14],3,0 ) << 8 );
    tmp.h_border         = d[i+15];
    tmp.v_border         = d[i+16];
    tmp.ilace            = get_bits( d[i+17],7,7 );
    tmp.stereo           = get_bits( d[i+17],6,5 );
    tmp.comp_sep         = get_bits( d[i+17],4,3 );
    tmp.serr_v_pol       = get_bits( d[i+17],2,2 );
    tmp.sync_comp_h_pol  = get_bits( d[i+17],1,1 );
    
    tmp.preferred_timing = 0;

    return tmp;
}

static HTX_BOOL parse_cea_block( EdidInfo e, HTX_BYTE *d, HTX_INT block_number )
{
    HTX_INT i;
    HTX_INT k; 
    struct MySADescriptorList sad={0};
    struct MySVDescriptorList svd={0};

    i = block_number;

    
    {
        HTX_INT j;
	HTX_BYTE sum = 0;

	for( j = i; j < ( i + 128 ); j++ ) {
	    sum += d[j];
	}

	if( sum ) {
	    printk(KERN_ERR "Check sum does not match, EDID information corrupted\n" );

	    return( 1 );
	}	
	else {		
	    printk(KERN_ERR "checksum ok\n" );
	    
	    e->cea->checksum = HTX_TRUE;
	}
    }

    e->cea->cea_rev        = d[i+1];
    e->cea->underscan      = get_bits( d[i+3],7,7 );
    e->cea->audio          = get_bits( d[i+3],6,6 );
    e->cea->YCC444         = get_bits( d[i+3],5,5 );
    e->cea->YCC422         = get_bits( d[i+3],4,4 );
    e->cea->native_formats = get_bits( d[i+3],3,0 );

    
    {
        HTX_INT j = i + 4;

	while( j < i+d[i+2] ) {
	    HTX_INT tag = get_bits( d[j], 7, 5 );

	    if( tag == 1 ) {
	        printk(KERN_ERR "Short Audio Descriptor\n" );
		
		sad.aud_format   = get_bits( d[j+1],6,3 );
		sad.max_num_chan = get_bits( d[j+1],2,0 ) + 1;
		sad.khz_192      = get_bits( d[j+2],6,6 );
		sad.khz_176      = get_bits( d[j+2],5,5 );
		sad.khz_96       = get_bits( d[j+2],4,4 );
		sad.khz_88       = get_bits( d[j+2],3,3 );
		sad.khz_48       = get_bits( d[j+2],2,2 );
		sad.khz_44       = get_bits( d[j+2],1,1 );
		sad.khz_32       = get_bits( d[j+2],0,0 );
		sad.sample_rates = get_bits( d[j+2],6,0 );

		if( sad.aud_format == 1 ) {
		    sad.unc_24bit    = get_bits( d[j+3],2,2 );
		    sad.unc_20bit    = get_bits( d[j+3],1,1 );
		    sad.unc_16bit    = get_bits( d[j+3],0,0 );
		    sad.sample_sizes = get_bits( d[j+3],2,0 );
		}
		else {
		    sad.comp_maxbitrate = d[j+3] * 8;
		}
		add_sad( sad, e );
	    }
	    else if( tag == 2 ) {
	        printk(KERN_ERR "Short Video Descriptor\n" );
		for( k = j + 1; k <= ( j + get_bits( d[j],4,0 ) ); k++ ) {
		    svd.vid_id = get_bits( d[k],6,0 );
		    svd.native = get_bits( d[k],7,7 );
		    add_svd( svd, e );
		}
	    }
	    else if( tag == 3 ) {
	        printk(KERN_ERR "Vendor Specific Data Block\n" );
		
		e->cea->cea->ieee_reg[1] = (HTX_WORD)( ( d[j+1] ) | ( d[j+2] << 8 ) );
		
		e->cea->cea->ieee_reg[0] = (HTX_WORD)( d[j+3] );
		e->cea->cea->hdmi_addr_a = get_bits( d[j+4], 7, 4 );
		e->cea->cea->hdmi_addr_b = get_bits( d[j+4], 3, 0 );
		e->cea->cea->hdmi_addr_c = get_bits( d[j+5], 7, 4 );
		e->cea->cea->hdmi_addr_d = get_bits( d[j+5], 3, 0 );

		if( get_bits(d[j],4,0) >= 6 ) {
		    e->cea->cea->supports_ai = get_bits( d[j+6], 7, 7 );
		    e->cea->cea->vsdb_ext[0] = get_bits( d[j+6], 6, 0 );

		    k = 1;
		     
		    while( ( k < ( get_bits( d[j],4,0 ) - 5 ) ) && ( k < 24 ) ) {
		        e->cea->cea->vsdb_ext[k] = d[j+6+k];
			k++;
		    }
		}
		
		if( ( e->cea->cea->ieee_reg[1] == 0x0C03 ) &&
		    ( e->cea->cea->ieee_reg[0] == 0x00 ) )
		    e->cea->cea->vsdb_hdmi = HTX_TRUE;
	    }	
	    else if( tag == 4 ) {
	        printk(KERN_ERR "Speaker Allocation Block\n" );
		e->cea->cea->spad->rlc_rrc = get_bits( d[j+1], 6, 6 );
		e->cea->cea->spad->flc_frc = get_bits( d[j+1], 5, 5 );
		e->cea->cea->spad->rc      = get_bits( d[j+1], 4, 4 );
		e->cea->cea->spad->rl_rr   = get_bits( d[j+1], 3, 3 );
		e->cea->cea->spad->fc      = get_bits( d[j+1], 2, 2 );
		e->cea->cea->spad->lfe     = get_bits( d[j+1], 1, 1 );
		e->cea->cea->spad->fl_fr   = get_bits( d[j+1], 0, 0 );
		
	    }
	    else if( tag == 7 ) {
	        printk(KERN_ERR "Reserved Block\n" );
		if( d[j+1] == 0x05 ) {
		    printk(KERN_ERR "Colorimetry Data Block\n" );
		    e->cea->cea->cdb->xvycc709 = get_bits( d[j+2], 1, 1 );
		    e->cea->cea->cdb->xvycc601 = get_bits( d[j+2], 0, 0 );
		    e->cea->cea->cdb->md = get_bits( d[j+3], 2, 0 );
		}
	    }
	    else if( ( tag == 0 ) || ( tag == 5 ) || ( tag == 6 ) )
	        printk(KERN_ERR "Reserved\n" );
	    else
	        printk(KERN_ERR "invalid tag in cea data block\n" );
	    
	    j += ( get_bits( d[j], 4, 0 ) + 1 );
	}
    }
    
    {
        HTX_INT j = d[i+2] + i;
	
	while( ( d[j] != 0x0 ) && ( j <= 0xED ) ) {
	    add_dtb( parse_dtb(j,d), e );
	    j += 18;
	}
    }

    return( HTX_FALSE );
}

static HTX_BOOL parse_0_block( EdidInfo e, HTX_BYTE *d )
{
    HTX_BYTE header[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    HTX_INT i, j;
    HTX_INT first_dtb, last_dtb;

    if( memcmp( header, d, 8 ) )
        e->edid_header = HTX_FALSE;
    else
        e->edid_header = HTX_TRUE;
	
    e->edid_extensions = d[0x7E];
    
    {
        HTX_INT i;
	HTX_BYTE sum = 0;

	for( i = 0; i < 128; i++ ) {
	    sum += d[i];
	}
 
	if( sum ) {
	    printk(KERN_ERR "Check sum does not match, EDID information corrupted\n" );
	    return( 1 );
	}
	else {
	    printk(KERN_ERR "checksum ok\n" );
	    e->edid_checksum = HTX_TRUE;
	}
    }
    
    e->vpi->manuf[0] = (HTX_BYTE)( get_bits( d[0x8],6,2 ) + 'A' - 1 );
    e->vpi->manuf[1] = (HTX_BYTE)( ( ( get_bits( d[0x8], 1, 0 ) << 3 )
				 | get_bits( d[9], 7, 5 ) ) + ( 'A' - 1 ) );
    e->vpi->manuf[2] = (HTX_BYTE)(get_bits( d[0x9], 4, 0) + 'A' - 1 );
    e->vpi->manuf[3] = '\0';
    
    e->vpi->prodcode[0] = d[ 0xA ];
    e->vpi->prodcode[1] = d[ 0xB ];

    
    
    e->vpi->serial[1] = ( HTX_WORD )d[0xC]|( HTX_WORD )( d[0xD] << 8 );
    
    e->vpi->serial[0] = ( HTX_WORD )d[0xE]|( HTX_WORD )( d[0xF] << 8 );

    e->vpi->week = ( HTX_INT )d[0x10];
    e->vpi->year = 1990 + ( HTX_INT )d[0x11];
	
    e->evr->ver = d[0x12];
    e->evr->rev = d[0x13];
    
    e->bdp->anadig = get_bits( d[0x14], 7, 7 );
    if( e->bdp->anadig ) {
        
        
        if( e->evr->rev == 3 ){
        e->bdp->dfp1x = get_bits( d[0x14], 0, 0 );
    }
        else if( e->evr->rev == 4 ) {
	    e->bdp->color_bit_depth = get_bits( d[0x14], 6, 4 );
	    e->bdp->interface_type = get_bits( d[0x14], 3, 0 );
	}
    }
    else {
        e->bdp->siglev    = get_bits( d[0x14], 6, 5 );
	e->bdp->setup     = get_bits( d[0x14], 4, 4 );
	e->bdp->sepsyn    = get_bits( d[0x14], 3, 3 );
	e->bdp->compsync  = get_bits( d[0x14], 2, 2 );
	e->bdp->sog       = get_bits( d[0x14], 1, 1 );
	e->bdp->vsync_ser = get_bits( d[0x14], 0, 0 );
    }
	
    e->bdp->maxh       = d[0x15];
    e->bdp->maxv       = d[0x16];
    e->bdp->dtc_gam    = d[0x17];
    e->bdp->standby    = get_bits( d[0x18], 7, 7 );
    e->bdp->suspend    = get_bits( d[0x18], 6, 6 );
    e->bdp->aovlp      = get_bits( d[0x18], 5, 5 );
    
    if( ( e->evr->rev == 3 ) || ( ( e->evr->rev == 4 ) && ( !e->bdp->anadig ) ) ) {
        e->bdp->disptype   = get_bits( d[0x18], 4, 3 );
	e->bdp->def_col_sp = get_bits( d[0x18], 2, 2 );
	e->bdp->pt_mode    = get_bits( d[0x18], 1, 1 );
	e->bdp->def_gtf    = get_bits( d[0x18], 0, 0 );
    }
    else if( e->evr->rev == 4 ) {
        e->bdp->color_encoding_format = get_bits( d[0x18], 4, 3 );
	e->bdp->def_col_sp            = get_bits( d[0x18], 2, 2 );
	e->bdp->pt_mode               = get_bits( d[0x18], 1, 1 );
	e->bdp->display_frequency     = get_bits( d[0x18], 0, 0 );
    }
    
    

    e->cc->rx = get_bits( d[0x19], 7, 6 ) | ( d[0x1B] << 2 );
    e->cc->ry = get_bits( d[0x19], 5, 4 ) | ( d[0x1C] << 2 );
    e->cc->gx = get_bits( d[0x19], 3, 2 ) | ( d[0x1D] << 2 );
    e->cc->gy = get_bits( d[0x19], 1, 0 ) | ( d[0x1E] << 2 );
    e->cc->bx = get_bits( d[0x19], 7, 6 ) | ( d[0x1F] << 2 );
    e->cc->by = get_bits( d[0x19], 5, 4 ) | ( d[0x20] << 2 );
    e->cc->wx = get_bits( d[0x19], 3, 2 ) | ( d[0x21] << 2 );
    e->cc->wy = get_bits( d[0x19], 1, 0 ) | ( d[0x22] << 2 );
	
    e->et->m720_400_70 = get_bits( d[0x23], 7, 7 );
    e->et->m720_400_88 = get_bits( d[0x23], 6, 6 );
    e->et->m640_480_60 = get_bits( d[0x23], 5, 5 );
    e->et->m640_480_67 = get_bits( d[0x23], 4, 4 );
    e->et->m640_480_72 = get_bits( d[0x23], 3, 3 );
    e->et->m640_480_75 = get_bits( d[0x23], 2, 2 );
    e->et->m800_600_56 = get_bits( d[0x23], 1, 1 );
    e->et->m800_600_60 = get_bits( d[0x23], 0, 0 );
    
    e->et->m800_600_72   = get_bits( d[0x24], 7, 7 );
    e->et->m800_600_75   = get_bits( d[0x24], 6, 6 );
    e->et->m832_624_75   = get_bits( d[0x24], 5, 5 );
    e->et->m1024_768_87  = get_bits( d[0x24], 4, 4 );
    e->et->m1024_768_60  = get_bits( d[0x24], 3, 3 );
    e->et->m1024_768_70  = get_bits( d[0x24], 2, 2 );
    e->et->m1024_768_75  = get_bits( d[0x24], 1, 1 );
    e->et->m1280_1024_75 = get_bits( d[0x24], 0, 0 );
    
    e->et->m1152_870_75 = get_bits( d[0x25], 7, 7 );
    
    e->et->res_7 = get_bits( d[0x25], 6, 6 );
    e->et->res_6 = get_bits( d[0x25], 5, 5 );
    e->et->res_5 = get_bits( d[0x25], 4, 4 );
    e->et->res_4 = get_bits( d[0x25], 3, 3 );
    e->et->res_3 = get_bits( d[0x25], 2, 2 );
    e->et->res_2 = get_bits( d[0x25], 1, 1 );
    e->et->res_1 = get_bits( d[0x25], 0, 0 );
	
    for( j = 0x26; j <= 0x34; j += 2 ) {
        struct MyStandardTimingList st;		
	st.hap   = d[j] * 8 + 248;
	st.imasp = get_bits( d[j+1], 7, 6 );
	st.ref   = get_bits( d[j+1], 5, 0 ) + 60;
	add_st( st, e );
    }

    first_dtb = 0x36;
    last_dtb  = 0x6C;
	
    for( i = first_dtb; i <= last_dtb; i += 0x12 ) {
        
        if( ( d[i] == 0x0 ) && ( d[i+1] == 0x0 ) ) {
	    
	    if( d[i+3] == 0xFF ) {
	        printk(KERN_ERR "Monitor S/N\n" );
		for( j = 0; j < 13; j++ ) {
		    e->mdb->mon_snum[j] = d[i + 5 + j];
		    if( d[i + 5 + j] == 0xA )
		        e->mdb->mon_name[j] = '\0';
		}
	    }
	    
	    if( d[i+3] == 0xFE ) {
	        printk(KERN_ERR "ASCII String\n" );
		for( j = 0; j < 13; j++ ) {				
		    e->mdb->ascii_data[j] = d[i + 5 + j];
		    if( d[i + 5 + j] == 0xA )
		        e->mdb->mon_name[j] = '\0';
		}
	    }
	    
	    if( d[i+3] == 0xFD ) {
	        printk(KERN_ERR "Range Limits\n" );
	        
	        if( e->evr->rev == 3 ) {
		    e->mdb->mrl_min_vrate   = d[i+5];
		    e->mdb->mrl_max_vrate   = d[i+6];
		    e->mdb->mrl_min_hrate   = d[i+7];
		    e->mdb->mrl_max_hrate   = d[i+8];
		    e->mdb->mrl_max_pix_clk = d[i+9] * 10;
		    if( d[i+10] == 0x2 ) {
		        e->mdb->mrl_gtf_support    = HTX_TRUE;
			e->mdb->mrl_gtf_start_freq = d[i+12] * 2000;
			e->mdb->mrl_gtf_c          = d[i+13] / 2;
			e->mdb->mrl_gtf_m          = d[i+14]|(d[i+15]<<8); 
			e->mdb->mrl_gtf_k          = d[i+16];
			e->mdb->mrl_gtf_j          = d[i+17] / 2;
		    }
		    else
		       e->mdb->mrl_gtf_support = HTX_FALSE;
		}
		if( e->evr->rev == 4 ) {
		    e->mdb->mrl_hrange_offset = get_bits( d[i+4], 3, 2 );
		    e->mdb->mrl_vrange_offset = get_bits( d[i+4], 1, 0 );
		    e->mdb->mrl_min_vrate     = d[i+5];
		    e->mdb->mrl_max_vrate     = d[i+6];
		    e->mdb->mrl_min_hrate     = d[i+7];
		    e->mdb->mrl_max_hrate     = d[i+8];
		    e->mdb->mrl_max_pix_clk   = d[i+9] * 10;
		    
		    if( d[i+10] == 0x4 ) {
		        
		        e->mdb->mrl_cvt_supported = e->bdp->display_frequency & HTX_TRUE;


			e->mdb->mrl_max_pix_clk = ( d[i+9] * 10 ) - ( get_bits( d[i+12], 7, 2 ) / 4 );
			e->mdb->mrl_max_active_lines = ( get_bits( d[i+12], 1, 0 ) << 8 ) | d[i+13];
			e->mdb->mrl_ar_43   = get_bits( d[i+14], 7, 7 );
			e->mdb->mrl_ar_169  = get_bits( d[i+14], 6, 6 );
			e->mdb->mrl_ar_1610 = get_bits( d[i+14], 5, 5 );
			e->mdb->mrl_ar_54   = get_bits( d[i+14], 4, 4 );
			e->mdb->mrl_ar_159  = get_bits( d[i+14], 3, 3 );
                           
			e->mdb->mrl_par = get_bits( d[i+15], 7, 5 );
			e->mdb->mrl_std_cvt_blanking = get_bits( d[i+15], 3, 3 );
			e->mdb->mrl_red_cvt_blanking = get_bits( d[i+15], 4, 4 );
			e->mdb->mrl_hshrink  = get_bits( d[i+16], 7, 7 );
			e->mdb->mrl_hstretch = get_bits( d[i+16], 6, 6 );
			e->mdb->mrl_vshrink  = get_bits( d[i+16], 5, 5 );
			e->mdb->mrl_vstretch = get_bits( d[i+16], 4, 4 );
			
			e->mdb->mrl_pref_vrefresh_rate = d[i+17];
		    }
		    else {
		       if( d[i+10] == 0x2 ) {
			   e->mdb->mrl_gtf_support    = HTX_TRUE;
			   e->mdb->mrl_gtf_start_freq = d[i+12] * 2000;
			   e->mdb->mrl_gtf_c          = d[i+13] / 2;
			   e->mdb->mrl_gtf_m          = d[i+14] | ( d[i+15] << 8 );
			   e->mdb->mrl_gtf_k          = d[i+16];
			   e->mdb->mrl_gtf_j          = d[i+17] / 2;
		       }
		       else {
			   e->mdb->mrl_gtf_support = HTX_FALSE;
		       }
                    }
                }
            }  
	    
	    if( d[i+3] == 0xFC ) {
	        printk(KERN_ERR "Monitor Name\n" );
		for( j = 0; j < 13; j++ ) {				
		    e->mdb->mon_name[j] = d[i + 5 + j];
		    if( d[i + 5 + j] == 0xA )
		        e->mdb->mon_name[j] = '\0';
		}
	    }
	    
	    if( d[i+3] == 0xFB ) {
	        printk(KERN_ERR "Additional Color PoHTX_INT Data\n" );
		e->mdb->cp_wpoint_index1 = d[i+5];
		e->mdb->cp_w_lb1         = d[i+6];
		e->mdb->cp_w_x1          = d[i+7];
		e->mdb->cp_w_y1          = d[i+8];
		e->mdb->cp_w_gam1        = d[i+9];
		if( d[i+10] != 0x00 ) {
		    e->mdb->cp_wpoint_index2 = d[i+10];
		    e->mdb->cp_w_lb2         = d[i+11];
		    e->mdb->cp_w_x2          = d[i+12];
		    e->mdb->cp_w_y2          = d[i+13];
		    e->mdb->cp_w_gam2        = d[i+14];
		}
	    }
	    
	    if( d[i+3] == 0xFA ) {
	        printk(KERN_ERR "Additional Standard Timing Definitions\n" );

		for( j = 5; j < 18; j += 2 ) {
		    struct MyStandardTimingList st;
		    st.hap   = ( d[i + j] / 28 ) - 31;
		    st.imasp = get_bits( d[i+j+1], 7, 6 );
		    st.ref   = get_bits( d[i+j+1], 5, 0 ) + 60;
		    add_st( st, e );
		}
	    }
	    
	    
	    if( d[i+3] == 0xF8 ) {
	        printk(KERN_ERR "CVT 3byte Timing \n");
             
		
		e->mdb->cvt1_Vactive_lines = ( get_bits( d[i+7], 7, 4) << 8 ) | d[i+6];
		e->mdb->cvt1_aspect_ratio  = get_bits( d[i+7], 3, 2 );
		e->mdb->cvt1_pref_vrate    = get_bits( d[i+8], 6, 5 );
		e->mdb->cvt1_50hz_sb = get_bits( d[i+8], 4, 4 );
		e->mdb->cvt1_60hz_sb = get_bits( d[i+8], 3, 3 );
		e->mdb->cvt1_75hz_sb = get_bits( d[i+8], 2, 2 );
		e->mdb->cvt1_85hz_sb = get_bits( d[i+8], 1, 1 );
		e->mdb->cvt1_60hz_rb = get_bits( d[i+8], 0, 0 );
             
		
		e->mdb->cvt2_Vactive_lines = ( get_bits( d[i+10], 7, 4 ) << 8 ) | d[i+9];
		e->mdb->cvt2_aspect_ratio  = get_bits( d[i+10], 3, 2 );
		e->mdb->cvt2_pref_vrate    = get_bits( d[i+11], 6, 5 );
		e->mdb->cvt2_50hz_sb = get_bits( d[i+11], 4, 4 );
		e->mdb->cvt2_60hz_sb = get_bits( d[i+11], 3, 3 );
		e->mdb->cvt2_75hz_sb = get_bits( d[i+11], 2, 2 );
		e->mdb->cvt2_85hz_sb = get_bits( d[i+11], 1, 1 );
		e->mdb->cvt2_60hz_rb = get_bits( d[i+11], 0, 0 );
             
		
		e->mdb->cvt3_Vactive_lines = ( get_bits( d[i+13], 7, 4 ) << 8 ) | d[i+12];
		e->mdb->cvt3_aspect_ratio  = get_bits( d[i+13], 3, 2 );
		e->mdb->cvt3_pref_vrate    = get_bits( d[i+14], 6, 5 );
		e->mdb->cvt3_50hz_sb = get_bits( d[i+14], 4, 4 );
		e->mdb->cvt3_60hz_sb = get_bits( d[i+14], 3, 3 );
		e->mdb->cvt3_75hz_sb = get_bits( d[i+14], 2, 2 );
		e->mdb->cvt3_85hz_sb = get_bits( d[i+14], 1, 1 );
		e->mdb->cvt3_60hz_rb = get_bits( d[i+14], 0, 0 );
             
		
		e->mdb->cvt4_Vactive_lines = ( get_bits( d[i+16], 7, 4 ) << 8 ) | d[i+15];
		e->mdb->cvt4_aspect_ratio  = get_bits( d[i+16], 3, 2 );
		e->mdb->cvt4_pref_vrate    = get_bits( d[i+17], 6, 5 );
		e->mdb->cvt4_50hz_sb = get_bits( d[i+17], 4, 4 );
		e->mdb->cvt4_60hz_sb = get_bits( d[i+17], 3, 3 );
		e->mdb->cvt4_75hz_sb = get_bits( d[i+17], 2, 2 );
		e->mdb->cvt4_85hz_sb = get_bits( d[i+17], 1, 1 );
		e->mdb->cvt4_60hz_rb = get_bits( d[i+17], 0, 0 );
	    }
          
	    
         
	    if( d[i+3] == 0xF7 ) {
	        e->mdb->et3_byte6  = d[i+6];
		e->mdb->et3_byte7  = d[i+7];
		e->mdb->et3_byte8  = d[i+8];
		e->mdb->et3_byte9  = d[i+9];
		e->mdb->et3_byte10 = d[i+10];
		e->mdb->et3_byte11 = get_bits( d[i+11], 7, 4 );
            }
	    
	    if( d[i+3] == 0x10 ) {
	        printk(KERN_ERR "Dummy Descriptor\n" );
	    }

	    
	    if( d[i+3] <= 0x0F ) {
	        printk(KERN_ERR "Manufacturer Defined Descriptor\n" );
		for( j = 0; j < 13; j++ ) {
		    e->mdb->ms_HTX_BYTE[j] = d[i+5+j];
		}
	    }
	}
	else {
	    printk(KERN_ERR "DTB\n" );

	    
	    add_dtb( parse_dtb( i, d ), e );
	}
    }

    return( HTX_FALSE );
}

static VendorProductId init_vendor_product_id( VendorProductId vpi )
{
    vpi = &cMyVendorProductId;

    vpi->manuf[0]    = 0;
    vpi->manuf[1]    = 0;
    vpi->manuf[2]    = 0;
    vpi->manuf[3]    = '\0';
    vpi->prodcode[0] = 0;
    vpi->prodcode[1] = 0;
    vpi->prodcode[2] = '\0';
    vpi->serial[0]   = 0;
    vpi->serial[1]   = 0;
    vpi->week        = 0;
    vpi->year        = 0;
    
    return vpi;
}

static EdidVerRev init_edid_ver_rev( EdidVerRev evr )
{
    evr = &cMyEdidVerRev;

    evr->ver = 0; 
    evr->rev = 0;	
    
    return evr;
}

static BasicDispParams init_basic_disp_params( BasicDispParams bdp )
{
    bdp = &cMyBasicDispParams;

    bdp->anadig     = HTX_FALSE;
    bdp->siglev     = 0;
    bdp->setup      = HTX_FALSE;
    bdp->sepsyn     = HTX_FALSE;
    bdp->compsync   = HTX_FALSE;
    bdp->sog        = HTX_FALSE;
    bdp->vsync_ser  = HTX_FALSE;
    bdp->dfp1x      = HTX_FALSE;
    bdp->maxh       = 0;
    bdp->maxv       = 0;
    bdp->dtc_gam    = 0;
    bdp->standby    = HTX_FALSE;
    bdp->suspend    = HTX_FALSE;
    bdp->aovlp      = HTX_FALSE;
    bdp->disptype   = 0;
    bdp->def_col_sp = HTX_FALSE;
    bdp->pt_mode    = HTX_FALSE;
    bdp->def_gtf    = HTX_FALSE;

    return bdp;
}

static ColorCharacteristics init_color_characteristics( ColorCharacteristics cc )
{	
    cc = &cMyColorCharacteristics;

    cc->rx = 0;
    cc->ry = 0;
    cc->gx = 0;
    cc->gy = 0;
    cc->bx = 0;
    cc->by = 0;
    cc->wx = 0;
    cc->wy = 0;

    return cc;
}

static EstablishedTiming init_established_timing( EstablishedTiming et )
{	
    et = &cMyEstablishedTiming;

    
    et->m720_400_70 = HTX_FALSE;
    et->m720_400_88 = HTX_FALSE;
    et->m640_480_60 = HTX_FALSE;
    et->m640_480_67 = HTX_FALSE;
    et->m640_480_72 = HTX_FALSE;
    et->m640_480_75 = HTX_FALSE;
    et->m800_600_56 = HTX_FALSE;
    et->m800_600_60 = HTX_FALSE;

    
    et->m800_600_72   = HTX_FALSE;
    et->m800_600_75   = HTX_FALSE;
    et->m832_624_75   = HTX_FALSE;
    et->m1024_768_87  = HTX_FALSE;
    et->m1024_768_60  = HTX_FALSE;
    et->m1024_768_70  = HTX_FALSE;
    et->m1024_768_75  = HTX_FALSE;
    et->m1280_1024_75 = HTX_FALSE;

    
    et->m1152_870_75 = HTX_FALSE;

    
    et->res_7 = HTX_FALSE;
    et->res_6 = HTX_FALSE;
    et->res_5 = HTX_FALSE;
    et->res_4 = HTX_FALSE;
    et->res_3 = HTX_FALSE;
    et->res_2 = HTX_FALSE;
    et->res_1 = HTX_FALSE;

    return et;
}

static MonitorDescriptorBlock init_monitor_descriptor_block( MonitorDescriptorBlock mdb )
{
    HTX_INT i;
    mdb = &cMyMonitorDescriptorBlock;

    for( i = 0; i < 13; i++ ) {	
        mdb->mon_snum[i]   = 0;
	mdb->ascii_data[i] = 0;
	mdb->mon_name[i]   = 0;
    }

    mdb->mon_snum[13]       = '\0';
    mdb->ascii_data[13]     = '\0';
    mdb->mon_name[13]       = '\0';
    mdb->mrl_min_vrate      = 0; 
    mdb->mrl_max_vrate      = 0; 
    mdb->mrl_min_hrate      = 0; 
    mdb->mrl_max_hrate      = 0; 
    mdb->mrl_max_pix_clk    = 0; 
    mdb->mrl_gtf_support    = 0; 
    mdb->mrl_bad_stf        = HTX_FALSE;
    mdb->mrl_reserved_11    = 0; 
    mdb->mrl_gtf_start_freq = 0; 
    mdb->mrl_gtf_c          = 0; 
    mdb->mrl_gtf_m          = 0; 
    mdb->mrl_gtf_k          = 0; 
    mdb->mrl_gtf_j          = 0; 
    mdb->cp_wpoint_index1   = 0; 
    mdb->cp_w_lb1           = 0;
    mdb->cp_w_x1            = 0;
    mdb->cp_w_y1            = 0;
    mdb->cp_w_gam1          = 0;
    mdb->cp_wpoint_index2   = 0; 
    mdb->cp_w_lb2           = 0; 
    mdb->cp_w_x2            = 0;
    mdb->cp_w_y2            = 0;
    mdb->cp_w_gam2          = 0;
    mdb->cp_bad_set         = HTX_FALSE; 
    mdb->cp_pad1            = 0; 
    mdb->cp_pad2            = 0; 
    mdb->cp_pad3            = 0; 
    mdb->st9_hap            = 0; 
    mdb->st9_imasp          = 0; 
    mdb->st9_ref            = 0; 
    mdb->st10_hap           = 0; 
    mdb->st10_imasp         = 0; 
    mdb->st10_ref           = 0; 
    mdb->st11_hap           = 0; 
    mdb->st11_imasp         = 0; 
    mdb->st11_ref           = 0; 
    mdb->st12_hap           = 0; 
    mdb->st12_imasp         = 0;
    mdb->st12_ref           = 0; 
    mdb->st13_hap           = 0; 
    mdb->st13_imasp         = 0; 
    mdb->st13_ref           = 0; 
    mdb->st14_hap           = 0; 
    mdb->st14_imasp         = 0; 
    mdb->st14_ref           = 0; 
    mdb->st_set             = HTX_FALSE; 

    for( i = 0; i< 13; i++ ) {	
        mdb->ms_HTX_BYTE[i] = 0; 
    }

    return mdb;
}

static SpadPayload init_spad_payload( SpadPayload sp )
{
    sp = &cMySpadPayload;

    sp->rlc_rrc = HTX_FALSE; 
    sp->flc_frc = HTX_FALSE; 
    sp->rc      = HTX_FALSE; 
    sp->rl_rr   = HTX_FALSE; 
    sp->fc      = HTX_FALSE; 
    sp->lfe     = HTX_FALSE; 
    sp->fl_fr   = HTX_FALSE; 

    return sp;
}

static ColorimetryDataBlock init_cdb( ColorimetryDataBlock cdb )
{
	cdb = &cMyColorimetryDataBlock;
	
	cdb->xvycc709 = HTX_FALSE;
	cdb->xvycc601 = HTX_FALSE;
	cdb->md = 0;

	return cdb;
}

static CeaDataBlock init_cea_data_block( CeaDataBlock cea )
{
    cea = &cMyCeaDataBlock;

    cea->spad         = NULL;
    cea->spad         = init_spad_payload( cea->spad );
    cea->cdb	      = NULL;
    cea->cdb	      = init_cdb( cea->cdb );
    cea->ieee_reg[0]  = 0;
    cea->ieee_reg[1]  = 0;
    cea->vsdb_hdmi    = HTX_FALSE;
    cea->vsdb_payload = NULL;
    cea->hdmi_addr_a  = 0; 
    cea->hdmi_addr_b  = 0; 
    cea->hdmi_addr_c  = 0; 
    cea->hdmi_addr_d  = 0; 

    {
        HTX_INT i;

	for( i = 0; i < 26; i++ )
	    cea->vsdb_ext[i] = 0; 
    }

    cea->supports_ai = HTX_FALSE; 

    return cea;
}

static CeaTimingExtension init_cea_timing_extension( CeaTimingExtension cea )
{
    cea = &cMyCeaTimingExtension;

    cea->cea            = NULL;
    cea->cea_rev        = 0; 
    cea->underscan      = HTX_FALSE; 
    cea->audio          = HTX_FALSE;
    cea->YCC444         = HTX_FALSE;
    cea->YCC422         = HTX_FALSE;
    cea->native_formats = 0; 
    cea->cea            = init_cea_data_block( cea->cea );
    cea->checksum       = 0;

    return cea;
}

static void add_st( struct MyStandardTimingList st, EdidInfo e )
{
    if( ( st_count ) < ST_MAX ) {
        e->st[st_count] = st;
	st_count++;
    }
    else {
        ;
    }
}

static void add_dtb( struct MyDetailedTimingBlockList dtb, EdidInfo e )
{
    if( ( dtd_count ) < DTD_MAX ) {
        e->dtb[dtd_count] = dtb;
	dtd_count++;
    }
    else {
        ;
    }
}

static void add_svd( struct MySVDescriptorList svd, EdidInfo e )
{
    if( ( svd_count ) < SVD_MAX ) {
        e->cea->cea->svd[svd_count] = svd;
	svd_count++;
    }
    else {
        ;
    }
}

static void add_sad( struct MySADescriptorList sad, EdidInfo e )
{
    if( ( sad_count ) < SAD_MAX ) {
        e->cea->cea->sad[sad_count] = sad;
	sad_count++;
    }
    else {
        ;
    }
}

static void delete_sts( EdidInfo e )
{
    HTX_INT i;

    for( i = 0; i < ST_MAX; i++ ) {
       e->st[i].hap   = 0;
       e->st[i].imasp = 0;
       e->st[i].ref   = 0;
    }
}

static void delete_dtbs( EdidInfo e )
{
    HTX_INT i;

    for( i = 0; i < DTD_MAX; i++ ) {
        e->dtb[i].preferred_timing = 0;
	e->dtb[i].pix_clk          = 0;  
	e->dtb[i].h_active         = 0; 
	e->dtb[i].h_blank          = 0; 
	e->dtb[i].h_act_blank      = 0; 
	e->dtb[i].v_active         = 0; 
	e->dtb[i].v_blank          = 0;
	e->dtb[i].v_act_blank      = 0; 
	e->dtb[i].hsync_offset     = 0; 
	e->dtb[i].hsync_pw         = 0; 
	e->dtb[i].vsync_offset     = 0; 
	e->dtb[i].vsync_pw         = 0; 
	e->dtb[i].h_image_size     = 0; 
	e->dtb[i].v_image_size     = 0; 
	e->dtb[i].h_border         = 0; 
	e->dtb[i].v_border         = 0; 
	e->dtb[i].ilace            = 0;
	e->dtb[i].stereo           = 0; 
	e->dtb[i].comp_sep         = 0; 
	e->dtb[i].serr_v_pol       = 0;
	e->dtb[i].sync_comp_h_pol  = 0;
    }
}

static void delete_svds( EdidInfo e )
{
    HTX_INT i;

    for( i = 0; i < SVD_MAX; i++ ) {
        e->cea->cea->svd[i].native = 0;
	e->cea->cea->svd[i].vid_id = 0;
    }
}

static void delete_sads( EdidInfo e )
{
    HTX_INT i;

    for( i = 0; i < SAD_MAX; i++ ) {
        e->cea->cea->sad[i].aud_format      = 0;
	e->cea->cea->sad[i].max_num_chan    = 0; 
	e->cea->cea->sad[i].rsvd1           = 0;
	e->cea->cea->sad[i].rsvd2           = 0;
	e->cea->cea->sad[i].khz_192         = 0;
	e->cea->cea->sad[i].khz_176         = 0;
	e->cea->cea->sad[i].khz_96          = 0;
	e->cea->cea->sad[i].khz_88          = 0;
	e->cea->cea->sad[i].khz_48          = 0;
	e->cea->cea->sad[i].khz_44          = 0;
	e->cea->cea->sad[i].khz_32          = 0;
	e->cea->cea->sad[i].unc_rsrvd       = 0; 
	e->cea->cea->sad[i].unc_24bit       = 0; 
	e->cea->cea->sad[i].unc_20bit       = 0; 
	e->cea->cea->sad[i].unc_16bit       = 0; 
	e->cea->cea->sad[i].comp_maxbitrate = 0; 
	e->cea->cea->sad[i].sample_sizes    = 0;
	e->cea->cea->sad[i].sample_rates    = 0;
    }
}

static HTX_INT get_bits( HTX_BYTE in, HTX_INT high_bit,HTX_INT low_bit )
{
    HTX_INT i;
    HTX_BYTE mask = 0;

    for( i = 0; i <= ( high_bit - low_bit ); i++ )
        mask += ( 1 << i );

    mask <<= low_bit;
    in &= mask;
    
    return (HTX_INT)( in >>= low_bit );
}

void clear_edid( EdidInfo e )
{
    delete_dtbs( e );
    delete_sads( e );
    delete_svds( e );
    delete_sts( e );

    st_count  = 0;    
    dtd_count = 0;
    sad_count = 0;
    svd_count = 0;
    
    init_edid_info( e );

    segment_number_counter = 0;
}

EdidInfo init_edid_info( EdidInfo edid )
{
    edid = &cMyEdidInfo;

    edid->vpi = NULL;
    edid->evr = NULL;
    edid->bdp = NULL;
    edid->cc  = NULL;
    edid->et  = NULL;
    edid->mdb = NULL;
    edid->cea = NULL;
	
    edid->edid_header     = HTX_FALSE; 
    edid->edid_extensions = 0; 
    edid->edid_checksum   = 0;
    edid->ext_block_count = 0;

    edid->vpi = init_vendor_product_id( edid->vpi );
    edid->evr = init_edid_ver_rev( edid->evr );
    edid->bdp = init_basic_disp_params( edid->bdp );
    edid->cc  = init_color_characteristics( edid->cc );
    edid->et  = init_established_timing( edid->et );
    edid->mdb = init_monitor_descriptor_block( edid->mdb );
    edid->cea = init_cea_timing_extension( edid->cea );

    return edid;
}


HTX_INT parse_edid( EdidInfo e, HTX_BYTE *d )
{
    HTX_INT i;
    HTX_BOOL zero_check = HTX_TRUE;

    for( i = 0; i < 256; i++ ) {
        if( d[i] != 0 ) {
	    zero_check = HTX_FALSE;
	}   
    }

    if( zero_check ) {
        printk(KERN_ERR "\nAll Zeros\n" );
	return( -1 );
    }     

    if( !( ( d[ 0 ] == 0 ) || ( d[ 0 ] == 2 ) || ( d[ 0 ] == 0xF0 ) || ( d[ 0x80 ] == 0 )
	   || ( d[ 0x80 ] == 2 ) || ( d[ 0x80 ] == 0xF0 ) ) ) {
        printk(KERN_ERR "Error: Trying to parse edid segment with no valid blocks" );
        return( -1 );

        
	
    }

    if( ( d[ 0 ] == 0 ) && ( d[ 1 ] == 0xFF ) ) {
        printk(KERN_ERR "Block 0 found\n" );
        if( parse_0_block( e, d ) ) {
	    return( -1 );
        }
printk(KERN_ERR "[Jackie] edid_extensions=%d\n", e->edid_extensions );
        if( e->edid_extensions <= 1 ) {
	    segment_number[ 0 ] = 0;
	}
        
	segment_number_counter = 0;    
    }
    
    if( d[ 0x00 ] == 0xF0 ) {
        parse_block_map( d, 0x00 );
    }

    if( d[ 0x80 ] == 0xF0 ) {
        parse_block_map( d, 0x80 );
    }

    if( e->edid_extensions > 0 ) {
        for( i = 0; i < 2; i++ ) {
	    if( d[ i * 0x80 ] == 0x2 ) {
	        printk(KERN_ERR "cea extension block %d found\n", i );
		if( parse_cea_block( e, d, i * 0x80 ) ) {
		    return( -1 );
		}
  
#if defined CEC_AVAILABLE
printk(KERN_ERR "[parse_edid] call cec_hpd()\n" );
		cec_hpd();
#endif
	    }	
	}
    }

    if( ( segment_number[ segment_number_counter ] == 0 ) && ( e->edid_extensions > 127 ) )
        return 0x40;
    else
        segment_number_counter++;


printk(KERN_ERR "[Jackie parse_edid]segment_number_counter=%d, edid_extensions=%d\n", segment_number_counter, e->edid_extensions);
for( i = 0; i < segment_number_counter; i++ )
{
	printk(KERN_ERR "%d ", segment_number[i]);
}
printk(KERN_ERR "\nfor loop end\n" );

#if EDID_WORKAROUND



printk(KERN_ERR "[Jackie parse_edid] segment_number[%d]=%d\n",segment_number_counter-1, segment_number[ segment_number_counter - 1 ]);

if((e->edid_extensions >1) && (segment_number[  segment_number_counter - 1  ] == 0))
{
	printk(KERN_ERR "[Jackie parse_edid] Warring !! segment_number_counter[%d]=%d\n", segment_number_counter - 1  , segment_number_counter);
	segment_number[  segment_number_counter - 1  ] = segment_number_counter;
}



#endif

    return segment_number[ segment_number_counter - 1 ];
}
