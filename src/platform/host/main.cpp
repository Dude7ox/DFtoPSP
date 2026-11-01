#include "../../core/Vfs.h"
#include "../../core/Bsa.h"
#include "../../core/Maps.h"
#include "../../core/Scene.h"
#include "../../core/SoftwareRenderer.h"
#include <cstdio>
#include <cstring>
using namespace dfpsp;
int main(int argc,char**argv){
    std::string root="data", block="B0000001.RDB", out="frame.bmp"; int w=480,h=272; bool layout=false; float cyaw=0.5f,cpitch=-0.1f; bool haveCam=false; Vec3 cpos;
    for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--root")&&i+1<argc)root=argv[++i];else if(!strcmp(argv[i],"--block")&&i+1<argc)block=argv[++i];else if(!strcmp(argv[i],"--out")&&i+1<argc)out=argv[++i];else if(!strcmp(argv[i],"--layout"))layout=true;else if(!strcmp(argv[i],"--width")&&i+1<argc)w=atoi(argv[++i]);else if(!strcmp(argv[i],"--height")&&i+1<argc)h=atoi(argv[++i]);else if(!strcmp(argv[i],"--yaw")&&i+1<argc)cyaw=atof(argv[++i]);else if(!strcmp(argv[i],"--pitch")&&i+1<argc)cpitch=atof(argv[++i]);else if(!strcmp(argv[i],"--cam")&&i+3<argc){haveCam=true;cpos={atof(argv[i+1]),atof(argv[i+2]),atof(argv[i+3])};i+=3;}}
    Vfs vfs; vfs.addRoot(root); vfs.addRoot(root+"/ARENA2"); vfs.loadModList(root); vfs.loadModList(".");
    Bsa arch3d,blocks,maps;
    if(!arch3d.open(vfs,"ARCH3D.BSA")){fprintf(stderr,"ARCH3D.BSA missing\n");return 1;}
    if(!blocks.open(vfs,"BLOCKS.BSA")){fprintf(stderr,"BLOCKS.BSA missing\n");return 1;}
    if(layout){ if(!maps.open(vfs,"MAPS.BSA")){fprintf(stderr,"MAPS.BSA missing\n");return 1;} DungeonLayout l; if(findDungeonByName(maps,"Evelara",l)){printf("%s region=%d ext=%d int=%d blocks=%zu\n",l.name.c_str(),l.region,l.exteriorId,l.interiorId,l.blocks.size());for(auto&b:l.blocks)printf("  %d %d %s%s\n",b.x,b.z,b.name.c_str(),b.start?" START":"");} return 0; }
    Palette pal; loadPalette(vfs,"ART_PAL.COL",pal);
    SceneBuilder sb(vfs,arch3d,blocks); Scene scene;
    if(!sb.loadBlock(block,scene)){fprintf(stderr,"failed to load %s\n",block.c_str());return 1;}
    fprintf(stderr,"loaded %s: %zu tris, %zu models, %zu flats\n",block.c_str(),scene.tris.size(),scene.modelCount,scene.flatCount);
    Camera cam;cam.pos=haveCam?cpos:scene.spawn+Vec3{0,64.f,0};cam.yaw=cyaw;cam.pitch=cpitch;
    Framebuffer fb;fb.resize(w,h);SoftwareRenderer rr(vfs,pal);rr.render(scene,cam,fb);
    if(!writeBmp(out,fb)){fprintf(stderr,"write failed\n");return 1;}printf("%s\n",out.c_str());return 0;
}
