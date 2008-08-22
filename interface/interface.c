/******************************************************************
 * CopyPolicy: GNU Lesser General Public License 2.1 applies
 * Copyright (C) 1998-2008 Monty xiphmont@mit.edu
 * 
 * Top-level interface module for cdrom drive access.  SCSI, ATAPI, etc
 *    specific stuff are in other modules.  Note that SCSI does use
 *    specialized ioctls; these appear in common_interface.c where the
 *    generic_scsi stuff is in scsi_interface.c.
 *
 ******************************************************************/

#include "low_interface.h"
#include "common_interface.h"
#include "utils.h"

static void _clean_messages(cdrom_drive *d){
  if(d){
    if(d->messagebuf)free(d->messagebuf);
    if(d->errorbuf)free(d->errorbuf);
    d->messagebuf=NULL;
    d->errorbuf=NULL;
  }
}

/* doubles as "cdrom_drive_free()" */
int cdda_close(cdrom_drive *d){
  if(d){
    if(d->opened)
      d->enable_cdda(d,0);

    _clean_messages(d);
    if(d->cdda_device_name)free(d->cdda_device_name);
    if(d->ioctl_device_name)free(d->ioctl_device_name);
    if(d->drive_model)free(d->drive_model);
    if(d->cdda_fd!=-1)close(d->cdda_fd);
    if(d->ioctl_fd!=-1 && d->ioctl_fd!=d->cdda_fd)close(d->ioctl_fd);
    if(d->private){
      if(d->private->sg_hd)free(d->private->sg_hd);
      free(d->private);
    }

    free(d);
  }
  return(0);
}

/* finish initializing the drive! */
int cdda_open(cdrom_drive *d){
  int ret;
  if(d->opened)return(0);

  switch(d->interface){
  case SGIO_SCSI_BUGGY1:  
  case SGIO_SCSI:  
  case GENERIC_SCSI:  
    if((ret=scsi_init_drive(d)))
      return(ret);
    break;
  case COOKED_IOCTL:  
    if((ret=cooked_init_drive(d)))
      return(ret);
    break;
#ifdef CDDA_TEST
  case TEST_INTERFACE:  
    if((ret=test_init_drive(d)))
      return(ret);
    break;
#endif
  default:
    cderror(d,"100: Interface not supported\n");
    return(-100);
  }
  
  /* Check TOC, enable for CDDA */
  
  /* Some drives happily return a TOC even if there is no disc... */
  {
     int i;
    for(i=0;i<d->tracks;i++)
      if(d->disc_toc[i].dwStartSector<0 ||
	 d->disc_toc[i+1].dwStartSector==0){
	d->opened=0;
	cderror(d,"009: CDROM reporting illegal table of contents\n");
	return(-9);
      }
  }

  if((ret=d->enable_cdda(d,1)))
    return(ret);
    
  /*  d->select_speed(d,d->maxspeed); most drives are full speed by default */
  if(d->bigendianp==-1)d->bigendianp=data_bigendianp(d);
  return(0);
}

int cdda_speed_set(cdrom_drive *d, int speed)
{
  if(d->set_speed)
    if(!d->set_speed(d,speed)) return 0;
      
  cderror(d,"405: Option not supported by drive\n");
  return -405;
}

long cdda_read(cdrom_drive *d, void *buffer, long beginsector, long sectors){
  if(d->opened){
    if(sectors>0){
      sectors=d->read_audio(d,buffer,beginsector,sectors);

      if(sectors>0){
	/* byteswap? */
	if(d->bigendianp==-1) /* not determined yet */
	  d->bigendianp=data_bigendianp(d);
	
	if(d->bigendianp!=bigendianp()){
	  int i;
	  u_int16_t *p=(u_int16_t *)buffer;
	  long els=sectors*CD_FRAMESIZE_RAW/2;
	  
	  for(i=0;i<els;i++)p[i]=swap16(p[i]);
	}
      }	
    }
    return(sectors);
  }
  
  cderror(d,"400: Device not open\n");
  return(-400);
}

/* Failure to clear the cache returns an error code but does not add a
   message to the message queue; this allows simpler code that assumes
   it can always issue a cache request, even if it's possible/likely
   it won't succeed */
int cdda_clear_cache(cdrom_drive *d, int lba, int sectors){
  if(!d->opened)return -400;
  if(!d->private->cache_clear) return -405;
  return d->private->cache_clear(d,lba,sectors);
}

/* Returns a (pessimistic) millisecond value for the duration of the
   last CDROM command; useful for looking for seek operations in the
   paranoia code. Does not return errors, only < 0 for no
   measurement.*/
int cdda_milliseconds(cdrom_drive *d){
  if(!d->opened)return -1;
  return d->private->last_milliseconds;
}

void cdda_verbose_set(cdrom_drive *d,int err_action, int mes_action){
  d->messagedest=mes_action;
  d->errordest=err_action;
}

extern char *cdda_messages(cdrom_drive *d){
  char *ret=d->messagebuf;
  d->messagebuf=NULL;
  return(ret);
}

extern char *cdda_errors(cdrom_drive *d){
  char *ret=d->errorbuf;
  d->errorbuf=NULL;
  return(ret);
}

