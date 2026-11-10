/* SPDX-License-Identifier: MPL-2.0
 * Small block-letter title, drawn as a terminal silhouette with a ruled shadow.
 * Inspired by the operator's ZWANG SuperCollider post, not an extra font asset.
 * The light sweep is a startup animation, never a fabricated progress meter.
 */
static uint32_t boot_active,boot_started,boot_last_frame;
#define XENOLITH_BOOT_MS 1400u

static const uint8_t boot_letters[8][5]={
    {99,54,28,54,99},     /* X */
    {127,96,124,96,127},  /* E */
    {99,115,107,103,99},  /* N */
    {62,99,99,99,62},     /* O */
    {96,96,96,96,127},    /* L */
    {127,28,28,28,127},   /* I */
    {127,28,28,28,28},    /* T */
    {99,99,127,99,99}     /* H */
};

static uint32_t boot_pixel(int32_t x,int32_t y)
{
    uint32_t col,letter,row;
    if(y<0||y>=40)return 0u;
    row=(uint32_t)y/8u;
    if(x<0||x>=189)return 0u;
    col=(uint32_t)x/3u;letter=col/8u;col%=8u;
    return letter<8u&&col<7u&&(boot_letters[letter][row]&(64u>>col));
}

static uint32_t boot_face(int32_t x,int32_t y)
{
    /* One extra pixel of horizontal ink; keep the letter gaps and counters. */
    return boot_pixel(x,y)||boot_pixel(x-1,y);
}

static uint32_t boot_meeting(uint32_t elapsed)
{
    if(elapsed<900u||elapsed>=1370u)return 0u;
    if(elapsed<950u)return (elapsed-900u)*256u/50u;
    return (1370u-elapsed)*256u/420u;
}

static void boot_lamp(uint32_t elapsed)
{
    uint32_t glow=boot_meeting(elapsed);
    uint16_t rule=RGB(72,12,12),corner=RGB(152,0,0);
    uint16_t lamp,inner;
    if(elapsed>=950u&&glow<64u)glow=64u;
    lamp=RGB(152u+glow*96u/256u,glow*152u/256u,glow*12u/256u);
    inner=RGB(104u+glow*144u/256u,12u+glow*180u/256u,4u+glow*24u/256u);
    cv_begin(216,24,C_BLACK);
    cv_line(20,11,86,11,rule);cv_line(130,11,195,11,rule);
    cv_line(20,8,20,14,corner);cv_line(195,8,195,14,corner);
    cv_line(25,14,78,14,RGB(40,8,8));cv_line(138,14,190,14,RGB(40,8,8));
    cv_line(86,7,86,15,corner);cv_line(130,7,130,15,corner);
    cv_line(86,11,96,11,corner);cv_line(120,11,130,11,corner);
    cv_line(98,11,108,1,lamp);cv_line(108,1,118,11,lamp);
    cv_line(98,11,108,21,lamp);cv_line(108,21,118,11,lamp);
    cv_line(103,11,108,6,inner);cv_line(108,6,113,11,inner);
    cv_line(103,11,108,16,inner);cv_line(108,16,113,11,inner);
    cv_rect(107,10,3,3,inner);
    cv_blit(12,155);
}

static void boot_title(uint32_t elapsed)
{
    int32_t x,y,sweep2=(int32_t)(elapsed*233u/950u)-44;
    uint32_t approach=elapsed<600u?0u:elapsed<950u?(elapsed-600u)*256u/350u:256u;
    /* Low green/blue content retains a saturated red on the small backlit LCD.
     * This is a diffusion-inspired light field, not a circuit simulation. */
    const uint32_t rust_r=136u,rust_g=8u,rust_b=12u;
    cv_begin(216,66,C_BLACK);
    for(y=0;y<46;y++)for(x=-1;x<196;x++){
        uint32_t face=boot_face(x,y),shadow=boot_face(x-3,y-3),outer=boot_face(x-6,y-6);
        uint16_t ink=0;
        if(face){
            int32_t yy=y-20;
            int32_t bend=yy/5+(yy*yy-160)*((int32_t)elapsed-700)/48000;
            int32_t position2=x<95?2*x:378-2*x;
            int32_t d;
            /* Mirrored fronts share the title's half-pixel centre. Curvature
             * dies away before they meet, so every row converges together. */
            bend=elapsed<950u?bend*(int32_t)(950u-elapsed)/950:0;
            d=(position2-sweep2)/2-bend;
            uint32_t heat=0u,blue=0u;
            /* A broad, fading red wake and a narrow blue front. Continuous
             * through all rows: no row masks, flashing bands or white crest. */
            if(d>=-28&&d<=8){
                int32_t q=d<0?256+d*256/28:256-d*256/8;
                heat=(uint32_t)(q*q)/256u;
            }
            if(d>=0&&d<=9){
                int32_t q=d<4?d*256/4:(9-d)*256/5;
                blue=(uint32_t)(q*q)/256u*approach/256u;
            }
            {
                uint32_t red=rust_r+heat*80u/256u;
                uint32_t green=rust_g,cyan=rust_b+heat*16u/256u;
                red=(red*(256u-blue)+48u*blue)/256u;
                green=(green*(256u-blue)+16u*blue)/256u;
                cyan=(cyan*(256u-blue)+216u*blue)/256u;
                /* The approaching fronts meet once; a soft elliptical blue
                 * afterglow spreads inside the existing letter silhouette. */
                if(elapsed>=900u&&elapsed<1370u){
                    int32_t cx=2*x-189;if(cx<0)cx=-cx;
                    int32_t cy=yy;if(cy<0)cy=-cy;
                    int32_t spatial=96-cx-cy*2/3;
                    int32_t energy=(int32_t)boot_meeting(elapsed);
                    if(spatial>0&&energy>0){
                        uint32_t glow=(uint32_t)(spatial*energy/96);
                        red=(red*(256u-glow)+88u*glow)/256u;
                        green=(green*(256u-glow)+24u*glow)/256u;
                        cyan=(cyan*(256u-glow)+248u*glow)/256u;
                    }
                }
                ink=RGB(red,green,cyan);
            }
        }else if(!face&&shadow&&(!boot_face(x-4,y-3)||!boot_face(x-2,y-3)||
                         !boot_face(x-3,y-4)||!boot_face(x-3,y-2))){
            ink=RGB(96,12,8);
        }else if(!face&&!shadow&&outer&&(!boot_face(x-7,y-6)||!boot_face(x-5,y-6)||
                         !boot_face(x-6,y-7)||!boot_face(x-6,y-5))){
            ink=RGB(120,20,12);
        }
        if(ink)cv_rect(13+x,8+y,1,1,ink);
    }
    cv_blit(12,90);
    boot_lamp(elapsed);
}

void app_splash(void)
{
    uint16_t rule=RGB(72,12,12),corner=RGB(152,0,0);
    lcd_fill(0,0,240,240,C_BLACK);
    cv_begin(240,32,C_BLACK);
    cv_line(8,8,231,8,rule);cv_line(8,8,8,31,rule);cv_line(231,8,231,31,rule);
    cv_line(12,12,28,12,corner);cv_line(12,12,12,24,corner);
    cv_rect(194,14,32,16,C_BLACK);cv_text(198,14,&FONT_XS,"XX29",RGB(128,56,48));
    cv_blit(0,0);
    lcd_fill(8,32,1,176,rule);lcd_fill(231,32,1,176,rule);
    cv_begin(240,32,C_BLACK);
    cv_line(8,0,8,23,rule);cv_line(231,0,231,23,rule);cv_line(8,23,231,23,rule);
    cv_line(211,19,227,19,corner);cv_line(227,7,227,19,corner);
    cv_blit(0,208);
    boot_title(0u);
}

/* Three-section sight gauge in the upper-right header margin.
 * Its frame and terminals are symmetric about (215.5,7).
 * Real coarse levels only: no percentage or inferred charging animation. */
static void draw_battery(void)
{
    int value=plat_battery();uint32_t i;uint16_t edge=RGB(112,56,56);
    cv_rect(200,0,32,15,C_BLACK);
    cv_line(200,7,203,7,edge);cv_line(228,7,231,7,edge);
    cv_line(203,4,203,10,edge);cv_line(228,4,228,10,edge);
    cv_line(203,4,205,2,edge);cv_line(203,10,205,12,edge);
    cv_line(226,2,228,4,edge);cv_line(226,12,228,10,edge);
    cv_line(205,2,226,2,edge);cv_line(205,12,226,12,edge);
    cv_line(207,4,224,4,RGB(60,22,18));cv_line(207,10,224,10,RGB(60,22,18));
    if(value==4){ /* USB host supplies power, not a claim that it is charging. */
        cv_rect(207,6,4,3,RGB(152,48,32));cv_rect(220,6,4,3,RGB(152,48,32));
        cv_line(213,7,218,7,RGB(190,110,72));cv_line(215,5,215,9,RGB(190,110,72));
        cv_line(216,5,216,9,RGB(190,110,72));
    }else if(value>=0&&value<=3){
        for(i=0;i<3u;i++)cv_rect(207+(int32_t)i*6,5,5,5,i<(uint32_t)value?RGB(163,71,58):RGB(40,12,12));
    }else cv_line(215,7,216,7,edge);
}

static void draw_panel_header(const char *title)
{
    int32_t width=text_w(&FONT_XS,title),x=(240-width)/2;
    cv_text(8,0,&FONT_XS,"XX29",C_DIM);
    if(x>44)cv_line(39,7,x-6,7,C_LINE);
    cv_text(x,0,&FONT_XS,title,C_AMB);
    if(x+width<195)cv_line(x+width+6,7,198,7,C_LINE);
    draw_battery();
}
