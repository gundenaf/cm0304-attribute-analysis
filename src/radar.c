typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
volatile u32 g_profile_counter = 0;
volatile u32 g_have_attrs = 0;
volatile u8 g_attrs[42];

static void pixel(u32 *buf,int stride,int w,int h,int x,int y,u32 rgb){
    if((unsigned)x>=(unsigned)w||(unsigned)y>=(unsigned)h)return;
    buf[(h-1-y)*stride+x]=rgb;
}
static void line(u32 *buf,int stride,int w,int h,int x0,int y0,int x1,int y1,u32 rgb){
    int dx=x1-x0,dy=y1-y0,sx,sy,err,e2;
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    sx=(x0<x1)?1:-1;sy=(y0<y1)?1:-1;err=dx-dy;
    for(int i=0;i<1500;i++){
        pixel(buf,stride,w,h,x0,y0,rgb);
        if(x0==x1&&y0==y1)break;
        e2=2*err;
        if(e2>-dy){err-=dy;x0+=sx;}
        if(e2<dx){err+=dx;y0+=sy;}
    }
}
static int average(const int *idx,int count){
    int sum=0;
    for(int k=0;k<count;k++)sum+=g_attrs[idx[k]];
    return (sum+count/2)/count;
}
/* Outfield order clockwise from top (matching FM examples):
   Defending, Physical, Speed, Creativity,
   Attacking, Technical, Aerial, Mental. */
static const int out_idx[8][6]={
    {17,21,32,-1,-1,-1}, {36,37,39,40,-1,-1}, {35,38,-1,-1,-1,-1},
    {18,26,29,-1,-1,-1}, {13,27,31,-1,-1,-1}, {12,22,-1,-1,-1,-1},
    {14,41,-1,-1,-1,-1}, {24,25,27,28,33,-1}
};
/* CM03/04 has neither Concentration nor Composure nor First Touch:
   decisions/technique are explicitly used as stand-ins. */
static const int gk_idx[8][6]={
    {10,6,-1,-1,-1,-1}, {36,37,41,39,40,-1}, {35,38,-1,-1,-1,-1},
    {24,25,27,28,32,-1}, {2,1,-1,-1,-1,-1}, {3,7,8,-1,-1,-1},
    {0,4,-1,-1,-1,-1}, {5,9,-1,-1,-1,-1}
};
/* 5x7 bitmap glyphs for ASCII A-Z and 0-9, each row stored in five bits. */
static const u8 FONT[36][7]={
 {14,17,17,31,17,17,17}, /* A */
 {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
 {31,16,16,30,16,16,16}, {14,17,16,23,17,17,14},
 {17,17,17,31,17,17,17}, {31,4,4,4,4,4,31},
 {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17},
 {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17},
 {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13},
 {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30},
 {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10},
 {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4},
 {31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14}, /* 0 */
 {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2},
 {31,16,16,30,1,1,30}, {14,16,16,30,17,17,14},
 {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
/* 2x legible 5x7 font; inverted DIB row order requires reversing the
   logical vertical order so labels appear right-side up on screen. */
static void text(u32 *buf,int stride,int w,int h,int x,int y,const char *s,u32 col){
    for(int i=0;s[i];i++){
        int ch=s[i],g=-1;
        if(ch>='A'&&ch<='Z')g=ch-'A';
        else if(ch>='0'&&ch<='9')g=26+ch-'0';
        if(g>=0){
            for(int row=0;row<7;row++)for(int c=0;c<5;c++){
                if(FONT[g][row]&(16>>c)) {
                    int py=y+(6-row)*2;
                    int px=x+c*2;
                    pixel(buf,stride,w,h,px,py,col);
                    pixel(buf,stride,w,h,px+1,py,col);
                    pixel(buf,stride,w,h,px,py+1,col);
                    pixel(buf,stride,w,h,px+1,py+1,col);
                }
            }
        }
        x+=12;
    }
}

__attribute__((noinline)) void radar_draw(void *surface){
    if(!surface||!g_profile_counter||!g_have_attrs)return;
    g_profile_counter--;
    u8 *s=(u8*)surface;
    u32 *buf=*(u32**)(s+0x64);
    int w=*(u16*)(s+0x7a),h=*(u16*)(s+0x7c),stride=*(u16*)(s+0x6e);
    if(!buf||w<900||h<600||w>3840||h>2160||stride<w||stride>w+64)return;
    int cx=w*175/1000,cy=h*590/1000,r=w*62/1000;
    if(r>h*140/1000)r=h*140/1000;
    if(r<40)return;
    static const int ux[8]={0,707,1000,707,0,-707,-1000,-707};
    /* Screen y is the inverse of the DIB row index: reverse the logical Y signs. */
    static const int uy[8]={1000,707,0,-707,-1000,-707,0,707};
    /* The 42-byte source is not a scouting-visibility mask. However, records
       with many zero/unavailable outfield ratings must never be rendered as
       if the full 8-axis profile were known. This conservative guard handles
       visibly incomplete records, not true hidden-value visibility. */
    static const u8 outfield_required[24] = {
        12,13,14,17,18,21,22,24,25,26,27,28,
        29,31,32,33,35,36,37,38,39,40,41,11
    };
    int valid=0;
    for(int i=0;i<24;i++) {
        int val=g_attrs[outfield_required[i]];
        if(val>=1 && val<=20) valid++;
    }
    if(valid < 19) {
        /* Unknown scouting values: show the panel but not a false polygon. */
        return;
    }
    /* GK detection is provisional until game metadata is decoded. */
    int is_gk=(g_attrs[0]+g_attrs[4]+g_attrs[6]>=43 && g_attrs[6]>=12);
    static const char *const out_labels[8]={"DEFENDING","PHYSICAL","SPEED","CREATIVITY","ATTACKING","TECHNICAL","AERIAL","MENTAL"};
    static const char *const gk_labels[8]={"ONE ON ONES","PHYSICAL","SPEED","MENTAL","COMMUNICATION","ECCENTRICITY","AERIAL","DISTRIBUTION"};
    int values[8];
    for(int i=0;i<8;i++){
        const int *ind=is_gk?gk_idx[i]:out_idx[i];
        int count=0;
        while(count<6&&ind[count]>=0)count++;
        values[i]=average(ind,count);
        if(values[i]<1)values[i]=1;
        if(values[i]>20)values[i]=20;
    }
    const u32 grid=0x00909090,poly=0x0000FFFF,letters=0x00FFFFFF;
    for(int k=1;k<=4;k++)for(int j=0;j<8;j++){
        int n=(j+1)&7;
        line(buf,stride,w,h,cx+ux[j]*r*k/4000,cy+uy[j]*r*k/4000,
             cx+ux[n]*r*k/4000,cy+uy[n]*r*k/4000,grid);
    }
    for(int j=0;j<8;j++){
        int ex=cx+ux[j]*r/1000,ey=cy+uy[j]*r/1000;
        line(buf,stride,w,h,cx,cy,ex,ey,grid);
        int n=(j+1)&7;
        int x0=cx+ux[j]*r*values[j]/20000,y0=cy+uy[j]*r*values[j]/20000;
        int x1=cx+ux[n]*r*values[n]/20000,y1=cy+uy[n]*r*values[n]/20000;
        line(buf,stride,w,h,x0,y0,x1,y1,poly);
        line(buf,stride,w,h,x0+1,y0,x1+1,y1,poly);
        const char *label=is_gk?gk_labels[j]:out_labels[j];
        int len=0;while(label[len])len++;
        int text_w=len?((len-1)*12+10):0;
        int lx=cx+ux[j]*(r+14)/1000-(text_w/2);
        int ly=cy+uy[j]*(r+16)/1000-7;
        /* Horizontal labels anchored by octagon direction, then clamped
           into the left profile panel so nothing spills outside it. */
        if(j==2||j==3||j==1) lx=cx+ux[j]*(r+12)/1000+2;
        if(j==6||j==5||j==7) lx=cx+ux[j]*(r+12)/1000-text_w-2;
        if(j==0||j==4) lx=cx-(text_w/2);
        {
            int panel_l=w*54/1000, panel_r=w*280/1000;
            int panel_t=cy-r-55, panel_b=cy+r+55;
            if(lx<panel_l) lx=panel_l;
            if(lx+text_w>panel_r) lx=panel_r-text_w;
            if(ly<panel_t) ly=panel_t;
            if(ly+16>panel_b) ly=panel_b-16;
        }
        text(buf,stride,w,h,lx,ly,label,letters);
    }
}
