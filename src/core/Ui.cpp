#include "Ui.h"
#include "Font.h"
#include <cstdio>
namespace dfpsp {
void MsgLog::push(const std::string& s){
    lines_.push_back(s);
    if(lines_.size()>kMax)lines_.erase(lines_.begin());
}
std::vector<std::string> charSheetLines(const PlayerState& p,const std::string& place){
    char line[64];std::vector<std::string> out;
    snprintf(line,sizeof(line),"Level %d  XP %d/%d",p.level,p.xp,p.level*100);out.push_back(line);
    snprintf(line,sizeof(line),"HP %d/%d  Armor %d",p.hp>0?p.hp:0,p.maxHp,p.armor);out.push_back(line);
    snprintf(line,sizeof(line),"Weapon skill %d%%",p.skill);out.push_back(line);
    snprintf(line,sizeof(line),"Lockpicking %d%%",p.lockpick);out.push_back(line);
    snprintf(line,sizeof(line),"Gold %d  Kills %d",p.gold,p.kills);out.push_back(line);
    if(!place.empty())out.push_back(place);
    return out;
}
std::vector<std::string> helpLines(){
    return {
        "Stick move    DPad look",
        "X door/pick   Square attack",
        "O run         Triangle noclip",
        "L/R noclip up and down",
        "Start save    Select window",
        "Home exit",
    };
}
std::vector<std::string> travelLines(const std::vector<LocationEntry>& e,int cursor,int maxLines,int& first,
                                      const std::string& currentName){
    std::vector<std::string> out;
    int total=(int)e.size(); if(total<=0){first=0;return out;}
    if(cursor<0)cursor=0; if(cursor>=total)cursor=total-1;
    first=cursor-maxLines/2; if(first<0)first=0;
    if(first+maxLines>total)first=total-maxLines; if(first<0)first=0;
    int n=total-first; if(n>maxLines)n=maxLines;
    for(int i=0;i<n;i++){
        const LocationEntry& le=e[(size_t)(first+i)];
        std::string line=(first+i==cursor?"> ":"  ")+le.name;
        if(le.name==currentName)line+=" *";
        out.push_back(line);
    }
    return out;
}
} // namespace
// --- character creation and inventory panels ---
#include <map>
namespace dfpsp {
static const u32 kInk=0xffffffff, kGold=0xff40e0ff, kDim=0xff909090, kCursor=0xff00ffff;
static std::map<std::string,ImgFile> g_imgs;
static ImgFile* img(const Vfs& vfs,const std::string& n){
    ImgFile& m=g_imgs[n];
    if(!m.count())m.open(vfs,n);
    return m.count()?&m:0;
}
static void blitImg(Framebuffer& fb,int dx,int dy,const Vfs& vfs,const std::string& n,int rec,
                    const Palette& pal,bool transparent){
    ImgFile* m=img(vfs,n);
    if(!m||!m->decode((size_t)rec))return;
    imgBlit(fb.color.data(),fb.w,fb.h,dx,dy,m->header((size_t)rec),pal,transparent);
}
static void box(Framebuffer& fb,int x,int y,int w,int h,u32 c){
    for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)
        if(i>=0&&j>=0&&i<fb.w&&j<fb.h)fb.color[(size_t)j*fb.w+i]=c;
}
static void center(const Framebuffer& fb,const Font& f,int y,const std::string& s,u32 c){
    drawText((Framebuffer&)fb,f,(fb.w-f.textWidth(s))/2,y,s,c);
}
static void wrap(const Font& f,const std::string& s,int maxW,std::vector<std::string>& out){
    std::string cur;
    for(size_t i=0;i<s.size();i++){
        cur+=s[i];
        if(s[i]==' '&&f.textWidth(cur)>maxW){
            while(!cur.empty()&&cur.back()==' ')cur.pop_back();
            out.push_back(cur);cur.clear();
        }
    }
    if(!cur.empty())out.push_back(cur);
}
static void listRows(Framebuffer& fb,const Font& f,int x,int y,const std::vector<std::string>& rows,
                     int cursor,int page,int visible){
    for(int i=0;i<visible&&page+i<(int)rows.size();i++){
        int r=page+i;
        std::string s=rows[(size_t)r];
        if(r==cursor){
            box(fb,x-2,y+i*9-1,f.textWidth(s)+4,10,0xff202040);
            drawText(fb,f,x,y+i*9,">",kCursor);
            drawText(fb,f,x+6,y+i*9,s,kInk);
        }else drawText(fb,f,x+6,y+i*9,s,kDim);
    }
}
void drawCharGenScreen(Framebuffer& fb,const CharGenFlow& f,
    const std::vector<Career>& classes,const BiogFile& biog,const TextRsc& text,
    const Vfs& vfs,const Palette& pal,const Font& body,const Font& title){
    box(fb,0,0,fb.w,fb.h,0xff000000);
    const CharacterDoc& d=f.doc;
    switch(f.step){
    case StepRace:{
        // Classic province map: the cursor roams Tamriel and the pixel it
        // stands on selects the race.
        blitImg(fb,0,0,vfs,"TMAP00I0.IMG",0,pal,false);
        int id=raceAt(vfs,f.raceX,f.raceY);
        for(int k=-3;k<=3;k++){
            if(k==0)continue;
            int x=f.raceX+k,y=f.raceY;
            if(x>=0&&x<fb.w)fb.color[(size_t)y*fb.w+x]=kCursor;
            x=f.raceX;y=f.raceY+k;
            if(y>=0&&y<fb.h)fb.color[(size_t)y*fb.w+x]=kCursor;
        }
        box(fb,2,2,316,12,0xb0000000);
        center(fb,title,4,"CHOOSE YOUR PROVINCE",kGold);
        if(id>=1&&id<=kRaceCount)
            center(fb,body,186,raceInfo(id)->name,kCursor);
        break;}
    case StepRaceConfirm:{
        blitImg(fb,0,0,vfs,"TMAP00I0.IMG",0,pal,false);
        box(fb,2,60,316,110,0xe0000000);
        drawText(fb,title,6,64,std::string("THE ")+raceInfo(d.race)->name,kGold);
        std::vector<std::string> ls;
        for(const std::string& l:text.lines(raceInfo(d.race)->descriptionId)){
            std::vector<std::string> w;wrap(body,l,308,w);
            for(auto&x:w)ls.push_back(x);
        }
        for(size_t i=0;i<ls.size()&&i<8;i++)drawText(fb,body,6,78+(int)i*9,ls[i],kInk);
        std::vector<std::string> rows={"Yes","No"};
        listRows(fb,body,270,150,rows,f.cursor,0,2);
        break;}
    case StepGender:{
        blitImg(fb,0,0,vfs,"CHGN00I0.IMG",0,pal,false);
        box(fb,90,70,140,40,0xd0000000);
        center(fb,title,74,"GENDER",kGold);
        std::vector<std::string> rows={kGenderNames[0],kGenderNames[1]};
        listRows(fb,body,110,86,rows,f.cursor,0,2);
        break;}
    case StepClassQuestions:{
        blitImg(fb,0,0,vfs,"CHGN00I0.IMG",0,pal,false);
        // The classic shows this on an unrolling scroll (SCRL00I0.GFX); the
        // scroll art is an RLE animation, so the text panel stands in for it.
        box(fb,0,110,320,88,0xe860a0c8);
        box(fb,0,110,320,2,0xff204060);
        ClassQuestion q;
        classQuestion(text,f.cq[f.cqAnswered<10?f.cqAnswered:9],q);
        char buf[48];
        std::snprintf(buf,sizeof(buf),"Question %d of 10",f.cqAnswered+1);
        drawText(fb,body,20,114,buf,0xff002040);
        int y=126;
        for(size_t i=0;i<q.text.size()&&i<2;i++){
            std::vector<std::string> w;wrap(body,q.text[i],280,w);
            for(auto&x:w){drawText(fb,body,20,y,x,0xff000000);y+=9;}
        }
        y+=4;
        for(int a=0;a<3&&y<196;a++){
            if(a==f.cursor)box(fb,18,y-1,284,10,0xff80c0e0);
            std::string line=std::string(1,(char)('a'+a))+") ";
            line+=q.answers[a].empty()?"":q.answers[a][0];
            drawText(fb,body,20,y,line,a==f.cursor?0xff000000:0xff103050);
            y+=11;
        }
        drawText(fb,body,4,4,"Answer all ten questions and we will recommend a class.",kInk);
        break;}
    case StepClassRecommend:{
        blitImg(fb,0,0,vfs,"CHGN00I0.IMG",0,pal,false);
        box(fb,30,20,260,160,0xe0000000);
        if(f.cqRecommended>=0&&f.cqRecommended<(int)classes.size()){
            char buf[96];
            std::snprintf(buf,sizeof(buf),"We recommend the class: %s",
                          classes[(size_t)f.cqRecommended].name.c_str());
            center(fb,title,26,buf,kGold);
            std::vector<std::string> ls;
            for(const std::string& l:text.lines(2100+f.cqRecommended)){
                std::vector<std::string> w;wrap(body,l,250,w);
                for(auto&x:w)ls.push_back(x);
            }
            for(size_t i=0;i<ls.size()&&i<12;i++)drawText(fb,body,36,44+(int)i*9,ls[i],kInk);
        }else{
            center(fb,title,26,"No class matched your answers",kGold);
        }
        std::vector<std::string> rows={"Accept","Choose manually"};
        listRows(fb,body,150,156,rows,f.cursor,0,2);
        break;}
    case StepClass:{
        blitImg(fb,0,0,vfs,"CHGN00I0.IMG",0,pal,false);
        box(fb,2,4,140,180,0xd0000000);
        drawText(fb,title,6,6,"CLASS",kGold);
        std::vector<std::string> rows;
        for(size_t i=0;i<classes.size();i++)rows.push_back(classes[i].name);
        rows.push_back("Custom...");
        int vis=18;
        listRows(fb,body,6,18,rows,f.cursor,f.page,vis);
        if(f.cursor==rows.size()-1||f.page+vis>=rows.size()){}
        box(fb,146,4,172,180,0xd0000000);
        int id=2100+(f.cursor<(int)classes.size()?f.cursor:0);
        std::vector<std::string> ls;
        for(const std::string& l:text.lines(id)){
            std::vector<std::string> w;wrap(body,l,166,w);
            for(auto&x:w)ls.push_back(x);
        }
        for(size_t i=0;i<ls.size()&&i<19;i++)drawText(fb,body,150,8+(int)i*9,ls[i],kInk);
        break;}
    case StepCustom:{
        blitImg(fb,0,0,vfs,"CUST00I0.IMG",0,pal,false);
        box(fb,4,40,200,140,0xd0000000);
        std::vector<std::string> rows;
        char buf[64];
        std::snprintf(buf,sizeof(buf),"Hit points per level: %d",d.career.hpPerLevel);
        rows.push_back(buf);
        for(int i=0;i<3;i++)rows.push_back(std::string("Primary ")+(d.career.primary[i]>=0?kSkillNames[d.career.primary[i]]:"(none)"));
        for(int i=0;i<3;i++)rows.push_back(std::string("Major   ")+(d.career.major[i]>=0?kSkillNames[d.career.major[i]]:"(none)"));
        for(int i=0;i<6;i++)rows.push_back(std::string("Minor   ")+(d.career.minor[i]>=0?kSkillNames[d.career.minor[i]]:"(none)"));
        rows.push_back("Special Advantages...");
        rows.push_back("Special Disadvantages...");
        rows.push_back("-- Done --");
        listRows(fb,body,8,44,rows,f.cursor,0,16);
        drawText(fb,title,8,20,"CUSTOM CLASS",kGold);
        drawText(fb,body,8,190,"Left/Right change, confirm to pick",kDim);
        {
            int pts=difficultyPoints(d);
            char pb[48];
            box(fb,208,40,108,142,0xd0000000);
            std::snprintf(pb,sizeof(pb),"Difficulty: %d",pts);
            drawText(fb,body,212,44,pb,pts<0||pts>40?kCursor:kGold);
            int py=58;
            for(size_t i=0;i<d.picks.size()&&py<176;i++){
                std::string nm=d.picks[i].dis?"-":"+";
                nm+=advLabel(d.picks[i].dis,d.picks[i].prim);
                if(d.picks[i].sec>=0){
                    nm+=" (";
                    nm+=advSubLabel(d.picks[i].dis,d.picks[i].prim,d.picks[i].sec);
                    nm+=")";
                }
                std::snprintf(pb,sizeof(pb)," %d",d.picks[i].cost);
                nm+=pb;
                drawText(fb,body,212,py,nm.substr(0,17),d.picks[i].dis?kCursor:kDim);
                py+=9;
            }
        }
        break;}
    case StepAdvList:
    case StepAdvPrimary:
    case StepAdvSecondary:{
        blitImg(fb,0,0,vfs,"CUST00I0.IMG",0,pal,false);
        box(fb,4,14,240,170,0xe0000000);
        char buf[80];
        if(f.step==StepAdvList){
            std::snprintf(buf,sizeof(buf),"SPECIAL %s",f.advDis?"DISADVANTAGES":"ADVANTAGES");
            drawText(fb,title,8,18,buf,kGold);
            std::vector<std::string> rows;
            for(size_t i=0;i<d.picks.size();i++){
                if(d.picks[i].dis!=f.advDis)continue;
                std::string nm=advLabel(f.advDis,d.picks[i].prim);
                if(d.picks[i].sec>=0){
                    nm+=" (";nm+=advSubLabel(f.advDis,d.picks[i].prim,d.picks[i].sec);nm+=")";
                }
                std::snprintf(buf,sizeof(buf),"  %d pts",d.picks[i].cost);
                nm+=buf;
                rows.push_back(nm);
            }
            rows.push_back("-- Add --");
            listRows(fb,body,8,34,rows,f.cursor,0,14);
            std::snprintf(buf,sizeof(buf),"Difficulty points: %d  (limit -12..40)",difficultyPoints(d));
            drawText(fb,body,8,172,buf,kDim);
        }else{
            bool prim=f.step==StepAdvPrimary;
            std::snprintf(buf,sizeof(buf),"ADD %s %s",f.advDis?"DISADVANTAGE":"ADVANTAGE",
                          prim?"":"EFFECT");
            drawText(fb,title,8,18,buf,kGold);
            int p=f.cursor;
            std::vector<std::string> rows;
            if(prim){
                for(int i=0;i<advCount(f.advDis);i++){
                    std::snprintf(buf,sizeof(buf),"%-30s %3d pts",advLabel(f.advDis,i),
                                  advSubCount(f.advDis,i)>0?advCost(f.advDis,i,0):advCost(f.advDis,i,0));
                    rows.push_back(buf);
                }
            }else{
                int ns=advSubCount(f.advDis,p);
                for(int i=0;i<ns;i++){
                    std::snprintf(buf,sizeof(buf),"%-30s %3d pts",advSubLabel(f.advDis,p,i),
                                  advCost(f.advDis,p,i));
                    rows.push_back(buf);
                }
            }
            listRows(fb,body,8,34,rows,prim?f.cursor:f.subCursor,0,14);
        }
        break;}
    case StepBiography:{
        blitImg(fb,0,0,vfs,"CHGN00I0.IMG",0,pal,false);
        box(fb,20,16,280,168,0xd0000000);
        char buf[32];
        std::snprintf(buf,sizeof(buf),"BIOGRAPHY %d/12",f.subCursor+1);
        drawText(fb,title,24,20,buf,kGold);
        if(f.subCursor<(int)biog.questions.size()){
            const BiogQuestion& q=biog.questions[(size_t)f.subCursor];
            int y=34;
            for(size_t i=0;i<q.text.size()&&i<2;i++){drawText(fb,body,24,y,q.text[i],kInk);y+=9;}
            y+=4;
            std::vector<std::string> rows;
            for(size_t i=0;i<q.answers.size();i++)rows.push_back(q.answers[i].text);
            listRows(fb,body,24,y,rows,f.cursor,0,11);
        }
        break;}
    case StepBonusStats:{
        blitImg(fb,0,0,vfs,"CHAR02I0.IMG",0,pal,false);
        box(fb,4,14,120,90,0xc8000000);
        std::vector<std::string> rows;
        char buf[48];
        for(int i=0;i<StatCount;i++){
            std::snprintf(buf,sizeof(buf),"%-4s %3d",kStatShort[i],d.attributes[i]);
            rows.push_back(buf);
        }
        listRows(fb,body,8,18,rows,f.cursor,0,8);
        std::snprintf(buf,sizeof(buf),"Bonus points: %d",d.bonusPool);
        drawText(fb,body,8,108,buf,d.bonusPool?kCursor:kDim);
        std::snprintf(buf,sizeof(buf),"HP %d  SP %d",d.maxHp,d.spellPoints());
        drawText(fb,body,8,118,buf,kInk);
        std::snprintf(buf,sizeof(buf),"Enc %d  Res %d",d.maxEncumbrance(),d.magicResist());
        drawText(fb,body,8,128,buf,kInk);
        break;}
    case StepBonusSkills:{
        blitImg(fb,0,0,vfs,"CHAR03I0.IMG",0,pal,false);
        box(fb,4,14,180,130,0xc8000000);
        std::vector<std::string> rows;
        char buf[64];
        int list[12],group[12],n=0;
        for(int i=0;i<3;i++)if(d.career.primary[i]>=0){list[n]=d.career.primary[i];group[n]=0;n++;}
        for(int i=0;i<3;i++)if(d.career.major[i]>=0){list[n]=d.career.major[i];group[n]=1;n++;}
        for(int i=0;i<6;i++)if(d.career.minor[i]>=0){list[n]=d.career.minor[i];group[n]=2;n++;}
        for(int i=0;i<n;i++){
            std::snprintf(buf,sizeof(buf),"%-16s %3d  pool %d",kSkillNames[list[i]],d.skillValues[list[i]],d.skillPool[group[i]]);
            rows.push_back(buf);
        }
        listRows(fb,body,8,18,rows,f.cursor,0,12);
        drawText(fb,body,8,150,"Right spends a group point, Left returns it",kDim);
        break;}
    case StepFace:{
        blitImg(fb,0,0,vfs,"CHAR01I0.IMG",0,pal,false);
        const RaceInfo* r=raceInfo(d.race);
        blitImg(fb,130,40,vfs,d.gender==1?r->headsFemale:r->headsMale,f.cursor,pal,true);
        char buf[32];
        std::snprintf(buf,sizeof(buf),"FACE %d/10",f.cursor+1);
        center(fb,title,150,buf,kGold);
        drawText(fb,body,110,90,"<",kCursor);
        drawText(fb,body,200,90,">",kCursor);
        break;}
    case StepName:{
        blitImg(fb,0,0,vfs,"CHAR00I0.IMG",0,pal,false);
        box(fb,80,40,160,110,0xc8000000);
        center(fb,title,44,"NAME",kGold);
        listRows(fb,body,90,58,f.nameList,f.cursor,0,8);
        drawText(fb,body,80,156,"Select resets to a new list",kDim);
        break;}
    case StepSummary:{
        blitImg(fb,0,0,vfs,"CHAR04I0.IMG",0,pal,false);
        box(fb,60,20,200,160,0xd0000000);
        char buf[80];
        int y=26;
        drawText(fb,title,64,y,d.name.c_str(),kGold);y+=12;
        std::snprintf(buf,sizeof(buf),"%s %s of class %s",kGenderNames[d.gender],
                      raceInfo(d.race)->name,d.career.name.c_str());
        drawText(fb,body,64,y,buf,kInk);y+=10;
        std::snprintf(buf,sizeof(buf),"Level %d  HP %d/%d  Gold %d",d.level,d.hp,d.maxHp,d.gold);
        drawText(fb,body,64,y,buf,kInk);y+=10;
        for(int i=0;i<StatCount;i+=2){
            std::snprintf(buf,sizeof(buf),"%s %3d  %s %3d",kStatShort[i],d.attributes[i],
                          kStatShort[i+1],d.attributes[i+1]);
            drawText(fb,body,64,y,buf,kInk);y+=9;
        }
        y+=2;
        int shown=0;
        for(int i=0;i<35&&shown<6;i++)if(d.skillValues[i]>0){
            std::snprintf(buf,sizeof(buf),"%-16s %3d",kSkillNames[i],d.skillValues[i]);
            drawText(fb,body,64,y,buf,kDim);y+=9;shown++;
        }
        {
            static const char* kReflexNames[5]={"Very High","High","Average","Low","Very Low"};
            char fb2[64];
            int rx=f.cursor==0?1:0,rr=f.cursor==1?1:0;
            std::snprintf(fb2,sizeof(fb2),"Face %c %d %c",rx?'<':' ',d.faceIndex+1,rx?'>':' ');
            drawText(fb,body,64,y,fb2,rx?kCursor:kDim);y+=9;
            int rf=d.reflexes>=0&&d.reflexes<5?d.reflexes:2;
            std::snprintf(fb2,sizeof(fb2),"Reflexes %c %s %c",rr?'<':' ',kReflexNames[rf],rr?'>':' ');
            drawText(fb,body,64,y,fb2,rr?kCursor:kDim);y+=9;
            drawText(fb,body,64,y,f.cursor==2?"> Done":  "  Done",f.cursor==2?kCursor:kDim);
        }
        center(fb,body,190,"Confirm Done to begin the game",kCursor);
        break;}
    default:break;
    }
    if(!f.message.empty())drawText(fb,body,4,190,f.message,kCursor);
}
int inventoryCategoryCount(){return 4;}
const char* inventoryCategoryName(int c){
    static const char* n[4]={"Weapons & Armor","Magic Items","Clothing & Misc","Ingredients"};
    return n[c<0?0:(c>3?3:c)];
}
static bool catHas(int category,int group){
    switch(category){
    case 0:return group==2||group==3;
    case 1:return group==4||group==5||group==25;
    case 3:return group>=15&&group<=22;
    default:return !(catHas(0,group)||catHas(1,group)||catHas(3,group));
    }
}
std::vector<int> inventoryCategoryItems(const Inventory& inv,int category){
    std::vector<int> out;
    for(size_t i=0;i<inv.items.size();i++)
        if(catHas(category,inv.items[i].group()))out.push_back((int)i);
    return out;
}
void drawInventoryScreen(Framebuffer& fb,const Inventory& inv,const PaperDoll& doll,
    int faceIndex,int category,int cursor,const Vfs& vfs,const Palette& pal,
    const Font& body,int gold){
    blitImg(fb,0,0,vfs,"INVE00I0.IMG",0,pal,false);
    for(int c=0;c<4;c++){
        int x=4+c*78;
        if(c==category)box(fb,x,2,76,10,0xa0000000);
        drawText(fb,body,x,3,inventoryCategoryName(c),c==category?kCursor:kDim);
    }
    std::vector<int> ids=inventoryCategoryItems(inv,category);
    int page=cursor>=9?cursor-8:0;
    for(int i=0;i<9&&page+i<(int)ids.size();i++){
        const Item& it=inv.items[(size_t)ids[(size_t)page+i]];
        int y=20+i*17;
        if(page+i==cursor)box(fb,2,y-2,150,17,0xa0004040);
        Texture icon;
        if(loadTexture(vfs,iconArchive(it),iconRecord(it,it.slot),pal,icon))
            for(int py=0;py<icon.h&&py<16;py++)for(int px=0;px<icon.w&&px<16;px++){
                u8 a=icon.rgba[((size_t)py*icon.w+px)*4+3];
                if(!a)continue;
                int fx=4+px,fy=y+py;
                if(fx<fb.w&&fy<fb.h)fb.color[(size_t)fy*fb.w+fx]=
                    rgba(icon.rgba[((size_t)py*icon.w+px)*4],icon.rgba[((size_t)py*icon.w+px)*4+1],icon.rgba[((size_t)py*icon.w+px)*4+2]);
            }
        char buf[40];
        std::string nm=it.displayName();
        if(it.stack>1){std::snprintf(buf,sizeof(buf)," x%d",it.stack);nm+=buf;}
        if(it.slot!=SlotNone)nm="*"+nm;
        drawText(fb,body,24,y+3,nm.substr(0,24),page+i==cursor?kInk:kDim);
    }
    if(!ids.empty()){
        const Item& it=inv.items[(size_t)ids[(size_t)cursor]];
        char buf[48];
        int y=176;
        std::snprintf(buf,sizeof(buf),"Value %d  Weight %d.%d kg",it.value*it.stack,
                      it.weightQ*it.stack/4,(it.weightQ*it.stack%4)*25);
        drawText(fb,body,4,y,buf,kInk);
    }
    char buf[48];
    std::snprintf(buf,sizeof(buf),"Gold: %d",gold);
    drawText(fb,body,160,188,buf,kGold);
    std::snprintf(buf,sizeof(buf),"Carried %d.%d kg",inv.weightQ()/4,(inv.weightQ()%4)*25);
    drawText(fb,body,4,188,buf,kDim);
    doll.draw(fb,200,8,inv,faceIndex,pal);
}
} // namespace dfpsp
