library(ggplot2)
library(reshape2)
options(scipen=9999)
datDir<-"C:/Users/Owner/Documents/GitHub/Lysimeter/logger/data/20170712/"
datFiles<-grep(".csv",list.files(datDir),value=TRUE)
if(length(datFiles[grep("Rssi",datFiles)])>1){datFiles[-grep("Rssi",datFiles)]}
datFiles
setwd(datDir)
outdir<-paste(datDir,"output/",sep='')
dir.create(outdir)
nNodes<-length(datFiles)
Dat<-data.frame(NodeID=numeric(0),unixtime=numeric(0),BatV=numeric(0),BoardTemp=numeric(0),ch1Kg=numeric(0),ch1Temp=numeric(0),ch2Kg=numeric(0),ch2Temp=numeric(0),ch3Kg=numeric(0),ch3Temp=numeric(0),ch4Kg=numeric(0),ch4Temp=numeric(0),ch5Kg=numeric(0),ch5Temp=numeric(0),ch6Kg=numeric(0),ch6Temp=numeric(0),ch7Kg=numeric(0),ch7Temp=numeric(0),ch8Kg=numeric(0),ch8Temp=numeric(0),count=numeric(0),DateTime=character())
a=1
for(a in 1:nNodes){
  tempDat<-read.csv(datFiles[a],header=FALSE,colClasses = c("character",rep("numeric",20)))
  names(tempDat)<-c("NodeID","unixtime","BatV","BoardTemp","ch1Kg","ch1Temp","ch2Kg","ch2Temp","ch3Kg","ch3Temp","ch4Kg","ch4Temp","ch5Kg","ch5Temp","ch6Kg","ch6Temp","ch7Kg","ch7Temp","ch8Kg","ch8Temp","count")
  tempDat$DateTime<-as.POSIXct(tempDat$unixtime, origin="1970-01-01",tz="UTC") #rtc is set to UTC
  # head(tempDat)
  # tail(tempDat)
  Dat<-rbind(Dat,tempDat)
}
# tail(Dat)
# unique(Dat$NodeID)
Datm<-melt(data = Dat, id.vars = c("NodeID","unixtime","BatV","BoardTemp","count","DateTime"))
Datm$ch<-factor(substr(Datm$variable,3,3))
Datm$measure<-factor(substr(Datm$variable,4,stop=7))
# str(Datm)
Datm2<-melt(Datm,id.vars=c("NodeID","DateTime","BatV","BoardTemp","count",'ch','measure'),measure.vars="value")
# str(Datm2)
temp<-Datm2[Datm2$measure=="Temp",]
names(temp)[names(temp)=='value']<-"Temp"
kg<-Datm2[Datm2$measure=="Kg",]
names(kg)[names(kg)=='value']<-"Kg"

Dat2<-merge(temp[,-c(7,8)],kg[,-c(7,8)])
Dat2$NodeID<-as.factor(Dat2$NodeID)
Dat2$Kg<-Dat2$Kg/10000
Dat2<-Dat2[Dat2$DateTime>"2016-01-01",]
# str(Dat2)
# head(Dat2)
Dat2a<-arrange(Dat2,NodeID,ch,DateTime)
# head(Dat2a)
# unique(Dat2a$NodeID)

ggplot(Dat2a,aes(x=DateTime,y=BatV))+geom_point()+ylim(11.5,13.2)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a,aes(x=DateTime,y=BoardTemp))+geom_point()+ylim(-10,35)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a,aes(x=DateTime,y=count))+geom_point()+ylim(0,1000)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))

ggplot(Dat2a[Dat2a$DateTime>"2017-07-09",],aes(x=DateTime,y=BatV))+geom_point()+ylim(11.5,13.2)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$DateTime>"2017-07-09",],aes(x=DateTime,y=BoardTemp))+geom_point()+ylim(-10,35)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$DateTime>"2017-07-09",],aes(x=DateTime,y=count))+geom_point()+ylim(0,1000)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))

ggplot(Dat2a[Dat2a$NodeID=='100.1',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,22)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.2',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,22)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.3',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,22)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='200.5',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,20)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='200.6',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,20)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.7',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,20)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.8',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,20)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.9',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,20)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggplot(Dat2a[Dat2a$NodeID=='100.10',],aes(x=DateTime,y=Kg))+geom_point()+ylim(5,25)+facet_wrap(~ch)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))

ggplot(Dat2a,aes(x=DateTime,y=Kg))+geom_point(aes(color=ch),size=1)+ylim(5,25)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggsave(paste(outdir,"allpoints.png",sep=''))
ggplot(Dat2a[Dat2a$DateTime>'2017-07-11'&Dat2a$DateTime<'2017-07-12',],aes(x=DateTime,y=Kg))+geom_point(aes(color=ch),size=1)+ylim(5,25)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
ggsave(paste(outdir,"0711points.png",sep=''))


###below is code to convert to mm of water.  There is no error checking or data cleaning...rough!
# Dat2b<-ddply(Dat2a,.(NodeID,ch),mutate,Kgdiff=c(0,diff(Kg)))
# ggplot(Dat2b[Dat2b$DateTime>'2017-07-11 18:00',],aes(x=DateTime,y=Kgdiff))+geom_point(aes(color=ch),size=1)+ylim(-.5,.5)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# ggsave(paste(outdir,"0711diffpoints.png",sep=''))

# Dat3<-ddply(Dat2b,.(NodeID,ch),mutate,rm30min=rollapply(data=Kg,width=3,FUN=mean,fill=0,align="right"),rm1hr=rollapply(data=rm30min,width=2,FUN=mean,fill=0,align="right"),rm1day=rollapply(data=rm1hr,width=24,FUN=mean,fill=NA,align="right"))
# str(Dat3)
# # # need to write code to sum to hr/day
# Dat3<-ddply(Dat3,.(NodeID,ch),mutate,Kgd1hr=c(0,diff(rm1hr)),Kgd1day=c(0,diff(rm1day)))
# ggplot(Dat3[Dat3$DateTime>'2017-07-11 18:00',],aes(x=DateTime,y=rm1hr))+geom_point(aes(color=ch),size=1)+ylim(5,25)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# ggsave(paste(outdir,"0711points1hr.png",sep=''))
# ggplot(Dat3[Dat3$DateTime>'2017-07-11 18:00',],aes(x=DateTime,y=rm1hr))+geom_point(aes(color=ch),size=1)+ylim(5,25)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# ggsave(paste(outdir,"0711points1hr.png",sep=''))
# ggplot(Dat3[Dat3$DateTime>'2017-07-11 18:00'&Dat3$ch%in%c(1:6),],aes(x=DateTime,y=Kgd1hr))+geom_point(aes(color=ch),size=1)+ylim(-0.1,.1)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# ggsave(paste(outdir,"0711points1hrdiff.png",sep=''))
# # #1000kg/m3, .15r^2*pi
# kg2mm<-.001*(1/(pi*0.15^2))*1000
# mm2kg<-(1/1000)*(pi*0.15^2)*1000
# Dat3<-ddply(Dat3,.(NodeID,ch),mutate,ETmm1hr=Kgd1hr*kg2mm)
# ggplot(Dat3[Dat3$DateTime>'2017-07-11 18:00'&Dat3$ch%in%c(1:6),],aes(x=DateTime,y=ETmm1hr))+geom_point(aes(color=ch),size=1)+ylim(-1,1)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# Dat4<-ddply(Dat3[Dat3$DateTime>='2017-07-12 00:00'&Dat3$DateTime<'2017-07-13 00:01'&Dat3$ch%in%c(1:6),],.(NodeID,ch),summarise,ETsum=sum(ET1hr))
# ggplot(Dat4[Dat4$ch%in%c(1:6),],aes(x=ch,y=ETsum))+geom_bar(aes(fill=ch),size=1,stat='identity')+ylim(-10,10)+facet_wrap(~NodeID)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))
# ggsave(paste(outdir,"0712ETmm.png",sep=''))
