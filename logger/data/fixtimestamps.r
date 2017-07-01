wd<-"C:/Users/Owner/Documents/GitHub/Lysimeter/logger/data/20170629/"
files<-list.files(wd)
files<-files[grep(".csv",x=files)]
files<-files[grep("100",x=files)]
files
setwd(wd)
dir.create("old_ts")
file.copy(paste(wd,files,sep=''),paste(wd,"old_ts/",files,sep=''))
a=1
for(a in 1:length(files)){
	tmp<-read.csv(files[a],header=FALSE)
	tmp$V2<-tmp$V2+(60*60*8)
	write.table(tmp,files[a],col.names=FALSE,row.names=FALSE,sep=',')
}