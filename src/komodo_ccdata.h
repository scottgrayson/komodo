/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef H_KOMODOCCDATA_H
#define H_KOMODOCCDATA_H

struct komodo_ccdata *CC_data;
int32_t CC_firstheight;

bits256 iguana_merkle(bits256 *tree,int32_t txn_count)
{
    int32_t i,n=0,prev; uint8_t serialized[sizeof(bits256) * 2];
    if ( txn_count == 1 )
        return(tree[0]);
    prev = 0;
    while ( txn_count > 1 )
    {
        if ( (txn_count & 1) != 0 )
            tree[prev + txn_count] = tree[prev + txn_count-1], txn_count++;
        n += txn_count;
        for (i=0; i<txn_count; i+=2)
        {
            iguana_rwbignum(1,serialized,sizeof(*tree),tree[prev + i].bytes);
            iguana_rwbignum(1,&serialized[sizeof(*tree)],sizeof(*tree),tree[prev + i + 1].bytes);
            tree[n + (i >> 1)] = bits256_doublesha256(0,serialized,sizeof(serialized));
        }
        prev = n;
        txn_count >>= 1;
    }
    return(tree[n]);
}

int32_t komodo_MoMoMdata(char *hexstr,int32_t hexsize,struct komodo_ccdataMoMoM *mdata,char *symbol,int32_t kmdheight,int32_t notarized_height)
{
    uint8_t hexdata[8192]; struct komodo_ccdata *ccdata,*tmpptr; int32_t len,maxpairs,i,retval=-1,max,offset,starti,endi; bits256 *tree=0,tmp; uint256 MoMoM;
    starti = endi = offset = max = len = maxpairs = 0;
    hexstr[0] = 0;
    if ( sizeof(hexdata)*2+1 > hexsize )
    {
        fprintf(stderr,"hexsize.%d too small for %d\n",hexsize,(int32_t)sizeof(hexdata));
        return(-1);
    }
    memset(mdata,0,sizeof(*mdata));
    portable_mutex_lock(&KOMODO_CC_mutex);
    DL_FOREACH_SAFE(CC_data,ccdata,tmpptr)
    {
        if ( ccdata->MoMdata.height < kmdheight )
        {
            fprintf(stderr,"%s notarized.%d kmd.%d\n",ccdata->symbol,ccdata->MoMdata.notarized_height,ccdata->MoMdata.height);
            if ( strcmp(ccdata->symbol,symbol) == 0 )
            {
                if ( endi == 0 )
                {
                    len += iguana_rwnum(1,&hexdata[len],sizeof(ccdata->CCid),(uint8_t *)&ccdata->CCid);
                    endi = ccdata->MoMdata.height;
                }
                if ( (mdata->numpairs == 1 && notarized_height == 0) || ccdata->MoMdata.notarized_height <= notarized_height )
                {
                    starti = ccdata->MoMdata.height + 1;
                    break;
                }
                if ( mdata->numpairs >= maxpairs )
                {
                    maxpairs += 100;
                    mdata->pairs = (struct komodo_ccdatapair *)realloc(mdata->pairs,sizeof(*mdata->pairs)*maxpairs);
                    fprintf(stderr,"pairs reallocated to %p num.%d\n",mdata->pairs,mdata->numpairs);
                }
                mdata->pairs[mdata->numpairs].notarized_height = ccdata->MoMdata.notarized_height;
                mdata->pairs[mdata->numpairs].MoMoMoffset = offset;
                mdata->numpairs++;
            }
            if ( offset >= max )
            {
                max += 100;
                tree = (bits256 *)realloc(tree,sizeof(*tree)*max);
                fprintf(stderr,"tree reallocated to %p max.%d\n",tree,max);
            }
            memcpy(&tree[offset],&ccdata->MoMdata.MoM,sizeof(bits256));
            offset++;
            starti = ccdata->MoMdata.height;
        }
    }
    portable_mutex_unlock(&KOMODO_CC_mutex);
    mdata->kmdstarti = starti;
    mdata->kmdendi = endi;
    if ( starti != 0 && endi != 0 && endi >= starti )
    {
        if ( tree != 0 && offset > 0 )
        {
            tree = (bits256 *)realloc(tree,sizeof(*tree)*(offset * 3));
            tmp = iguana_merkle(tree,offset);
            memcpy(&MoMoM,&tmp,sizeof(MoMoM));
            mdata->MoMoM = MoMoM;
            mdata->MoMoMdepth = offset;
            if ( mdata->numpairs > 0 )
            {
                len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->kmdstarti);
                len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->kmdendi);
                len += iguana_rwbignum(1,&hexdata[len],sizeof(mdata->MoMoM),(uint8_t *)&mdata->MoMoM);
                len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->MoMoMdepth);
                len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->numpairs);
                for (i=0; i<mdata->numpairs; i++)
                {
                    if ( len + sizeof(uint32_t)*2 > sizeof(hexdata) )
                    {
                        fprintf(stderr,"%s %d %d i.%d of %d exceeds hexdata.%d\n",symbol,kmdheight,notarized_height,i,mdata->numpairs,(int32_t)sizeof(hexdata));
                        break;
                    }
                    len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->pairs[i].notarized_height);
                    len += iguana_rwnum(1,&hexdata[len],sizeof(uint32_t),(uint8_t *)&mdata->pairs[i].MoMoMoffset);
                }
                if ( i == mdata->numpairs && len*2+1 < hexsize )
                {
                    init_hexbytes_noT(hexstr,hexdata,len);
                    fprintf(stderr,"hexstr.(%s)\n",hexstr);
                    retval = 0;
                } else fprintf(stderr,"%s %d %d too much hexdata[%d] for hexstr[%d]\n",symbol,kmdheight,notarized_height,len,hexsize);
            }
        }
    }
    if ( tree != 0 )
    {
        fprintf(stderr,"free tree.%p\n",tree);
        free(tree);
    }
    return(retval);
}

int32_t komodo_rwccdata(char *thischain,int32_t rwflag,struct komodo_ccdata *ccdata,struct komodo_ccdataMoMoM *MoMoMdata)
{
    uint256 hash,zero; bits256 tmp; int32_t i; struct komodo_ccdata *ptr; struct notarized_checkpoint *np;
    if ( rwflag == 0 )
    {
        // load from disk
    }
    else
    {
        // write to disk
    }
    if ( ccdata->MoMdata.height > 0 && (CC_firstheight == 0 || ccdata->MoMdata.height < CC_firstheight) )
        CC_firstheight = ccdata->MoMdata.height;
    for (i=0; i<32; i++)
        tmp.bytes[i] = ((uint8_t *)&ccdata->MoMdata.MoM)[31-i];
    memcpy(&hash,&tmp,sizeof(hash));
    fprintf(stderr,"[%s] ccdata.%s id.%d notarized_ht.%d MoM.%s height.%d/t%d\n",ASSETCHAINS_SYMBOL,ccdata->symbol,ccdata->CCid,ccdata->MoMdata.notarized_height,hash.ToString().c_str(),ccdata->MoMdata.height,ccdata->MoMdata.txi);
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        if ( CC_data != 0 && (CC_data->MoMdata.height > ccdata->MoMdata.height || (CC_data->MoMdata.height == ccdata->MoMdata.height && CC_data->MoMdata.txi >= ccdata->MoMdata.txi)) )
        {
            printf("out of order detected? SKIP CC_data ht.%d/txi.%d vs ht.%d/txi.%d\n",CC_data->MoMdata.height,CC_data->MoMdata.txi,ccdata->MoMdata.height,ccdata->MoMdata.txi);
        }
        else
        {
            ptr = (struct komodo_ccdata *)calloc(1,sizeof(*ptr));
            *ptr = *ccdata;
            portable_mutex_lock(&KOMODO_CC_mutex);
            DL_PREPEND(CC_data,ptr);
            portable_mutex_unlock(&KOMODO_CC_mutex);
        }
    }
    else
    {
        if ( MoMoMdata != 0 && MoMoMdata->pairs != 0 )
        {
            for (i=0; i<MoMoMdata->numpairs; i++)
            {
                if ( (np= komodo_npptr(MoMoMdata->pairs[i].notarized_height)) != 0 )
                {
                    memset(&zero,0,sizeof(zero));
                    if ( memcmp(&np->MoMoM,&zero,sizeof(np->MoMoM)) == 0 )
                    {
                        np->MoMoM = MoMoMdata->MoMoM;
                        np->MoMoMdepth = MoMoMdata->MoMoMdepth;
                        np->MoMoMoffset = MoMoMdata->MoMoMoffset;
                        np->kmdstarti = MoMoMdata->kmdstarti;
                        np->kmdendi = MoMoMdata->kmdendi;
                    }
                    else if ( memcmp(&np->MoMoM,&MoMoMdata->MoMoM,sizeof(np->MoMoM)) != 0 || np->MoMoMdepth != MoMoMdata->MoMoMdepth || np->MoMoMoffset != MoMoMdata->MoMoMoffset || np->kmdstarti != MoMoMdata->kmdstarti || np->kmdendi != MoMoMdata->kmdendi )
                    {
                        fprintf(stderr,"preexisting MoMoM mismatch: %s (%d %d %d %d) vs %s (%d %d %d %d)\n",np->MoMoM.ToString().c_str(),np->MoMoMdepth,np->MoMoMoffset,np->kmdstarti,np->kmdendi,MoMoMdata->MoMoM.ToString().c_str(),MoMoMdata->MoMoMdepth,MoMoMdata->MoMoMoffset,MoMoMdata->kmdstarti,MoMoMdata->kmdendi);
                    }
                }
            }
        }
    }
}

#endif
