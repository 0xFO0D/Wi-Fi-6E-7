#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include "../../include/firmware/emlfm.h"
#include "../../include/core/wifi67.h"

struct wifi67_emlfm_region {
    void *vaddr;
    dma_addr_t paddr;
    size_t size;
    u32 flags;
};

struct wifi67_emlfm {
    spinlock_t lock;
    struct {
        struct wifi67_emlfm_region iram;
        struct wifi67_emlfm_region dram;
        struct wifi67_emlfm_region sram;
    } mem[WIFI67_MAX_RADIOS];
    
    struct {
        u32 version;
        u32 features;
        u32 radio_mask;
        u8 state;
    } fw[WIFI67_MAX_RADIOS];
    
    struct {
        struct completion ready;
        struct work_struct crash_work;
        u32 error_code;
        u32 crash_count;
    } status[WIFI67_MAX_RADIOS];
    
    struct {
        void *ringbuf;
        dma_addr_t ringbuf_paddr;
        size_t ringbuf_size;
        u32 read_idx;
        u32 write_idx;
        spinlock_t lock;
    } ipc[WIFI67_MAX_RADIOS];
};

static void wifi67_emlfm_handle_crash(struct work_struct *work)
{
    struct wifi67_emlfm *emlfm = container_of(work, struct wifi67_emlfm,
                                            status[0].crash_work);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&emlfm->lock, flags);
    
    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        if (!(emlfm->fw[i].radio_mask & BIT(i)))
            continue;

        if (emlfm->fw[i].state == WIFI67_FW_STATE_CRASHED) {
            wifi67_hw_reset_radio(priv, i);
            emlfm->status[i].crash_count++;
            emlfm->fw[i].state = WIFI67_FW_STATE_RESET;
        }
    }
    
    spin_unlock_irqrestore(&emlfm->lock, flags);
}

static irqreturn_t wifi67_emlfm_irq_handler(int irq, void *data)
{
    struct wifi67_emlfm *emlfm = data;
    u32 status, radio_id;
    
    status = wifi67_hw_read32(priv, WIFI67_REG_FW_STATUS);
    radio_id = FIELD_GET(WIFI67_FW_RADIO_ID_MASK, status);
    
    if (status & WIFI67_FW_IRQ_CRASH) {
        emlfm->fw[radio_id].state = WIFI67_FW_STATE_CRASHED;
        emlfm->status[radio_id].error_code = 
            wifi67_hw_read32(priv, WIFI67_REG_FW_ERROR);
        schedule_work(&emlfm->status[radio_id].crash_work);
    }
    
    if (status & WIFI67_FW_IRQ_READY) {
        emlfm->fw[radio_id].state = WIFI67_FW_STATE_READY;
        complete(&emlfm->status[radio_id].ready);
    }
    
    return IRQ_HANDLED;
}

static int wifi67_emlfm_alloc_region(struct wifi67_emlfm *emlfm,
                                   struct wifi67_emlfm_region *region,
                                   size_t size, u32 flags)
{
    region->vaddr = dma_alloc_coherent(priv->dev, size,
                                     &region->paddr, GFP_KERNEL);
    if (!region->vaddr)
        return -ENOMEM;
        
    region->size = size;
    region->flags = flags;
    return 0;
}

static void wifi67_emlfm_free_region(struct wifi67_emlfm *emlfm,
                                   struct wifi67_emlfm_region *region)
{
    if (region->vaddr) {
        dma_free_coherent(priv->dev, region->size,
                         region->vaddr, region->paddr);
        region->vaddr = NULL;
    }
}

int wifi67_emlfm_init(struct wifi67_priv *priv)
{
    struct wifi67_emlfm *emlfm;
    int i, ret;

    emlfm = kzalloc(sizeof(*emlfm), GFP_KERNEL);
    if (!emlfm)
        return -ENOMEM;

    spin_lock_init(&emlfm->lock);

    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        init_completion(&emlfm->status[i].ready);
        INIT_WORK(&emlfm->status[i].crash_work, wifi67_emlfm_handle_crash);
        spin_lock_init(&emlfm->ipc[i].lock);
        
        ret = wifi67_emlfm_alloc_region(emlfm, &emlfm->mem[i].iram,
                                      WIFI67_FW_IRAM_SIZE,
                                      WIFI67_FW_REGION_EXEC);
        if (ret)
            goto err_free;
            
        ret = wifi67_emlfm_alloc_region(emlfm, &emlfm->mem[i].dram,
                                      WIFI67_FW_DRAM_SIZE,
                                      WIFI67_FW_REGION_RW);
        if (ret)
            goto err_free;
            
        ret = wifi67_emlfm_alloc_region(emlfm, &emlfm->mem[i].sram,
                                      WIFI67_FW_SRAM_SIZE,
                                      WIFI67_FW_REGION_SHARED);
        if (ret)
            goto err_free;
            
        emlfm->ipc[i].ringbuf = dma_alloc_coherent(priv->dev,
                                                  WIFI67_IPC_RING_SIZE,
                                                  &emlfm->ipc[i].ringbuf_paddr,
                                                  GFP_KERNEL);
        if (!emlfm->ipc[i].ringbuf) {
            ret = -ENOMEM;
            goto err_free;
        }
        
        emlfm->ipc[i].ringbuf_size = WIFI67_IPC_RING_SIZE;
    }

    ret = request_irq(priv->pdev->irq, wifi67_emlfm_irq_handler,
                     IRQF_SHARED, "wifi67-fw", emlfm);
    if (ret)
        goto err_free;

    priv->emlfm = emlfm;
    return 0;

err_free:
    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].iram);
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].dram);
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].sram);
        if (emlfm->ipc[i].ringbuf)
            dma_free_coherent(priv->dev, emlfm->ipc[i].ringbuf_size,
                            emlfm->ipc[i].ringbuf,
                            emlfm->ipc[i].ringbuf_paddr);
    }
    kfree(emlfm);
    return ret;
}

void wifi67_emlfm_deinit(struct wifi67_priv *priv)
{
    struct wifi67_emlfm *emlfm = priv->emlfm;
    int i;

    if (!emlfm)
        return;

    free_irq(priv->pdev->irq, emlfm);

    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        cancel_work_sync(&emlfm->status[i].crash_work);
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].iram);
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].dram);
        wifi67_emlfm_free_region(emlfm, &emlfm->mem[i].sram);
        if (emlfm->ipc[i].ringbuf)
            dma_free_coherent(priv->dev, emlfm->ipc[i].ringbuf_size,
                            emlfm->ipc[i].ringbuf,
                            emlfm->ipc[i].ringbuf_paddr);
    }

    kfree(emlfm);
    priv->emlfm = NULL;
}

int wifi67_emlfm_load_fw(struct wifi67_priv *priv, u8 radio_id,
                        const char *name)
{
    struct wifi67_emlfm *emlfm = priv->emlfm;
    const struct firmware *fw;
    int ret;

    if (!emlfm || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    ret = request_firmware(&fw, name, priv->dev);
    if (ret)
        return ret;

    spin_lock_irq(&emlfm->lock);
    
    if (emlfm->fw[radio_id].state != WIFI67_FW_STATE_RESET) {
        ret = -EBUSY;
        goto out;
    }

    ret = wifi67_hw_load_fw(priv, radio_id, fw->data, fw->size,
                           emlfm->mem[radio_id].iram.paddr,
                           emlfm->mem[radio_id].dram.paddr,
                           emlfm->mem[radio_id].sram.paddr);
    if (ret)
        goto out;

    emlfm->fw[radio_id].state = WIFI67_FW_STATE_LOADED;
    emlfm->fw[radio_id].radio_mask |= BIT(radio_id);

out:
    spin_unlock_irq(&emlfm->lock);
    release_firmware(fw);
    return ret;
}

int wifi67_emlfm_start_fw(struct wifi67_priv *priv, u8 radio_id)
{
    struct wifi67_emlfm *emlfm = priv->emlfm;
    unsigned long flags;
    int ret;

    if (!emlfm || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&emlfm->lock, flags);
    
    if (emlfm->fw[radio_id].state != WIFI67_FW_STATE_LOADED) {
        ret = -EINVAL;
        goto out;
    }

    reinit_completion(&emlfm->status[radio_id].ready);
    
    ret = wifi67_hw_start_fw(priv, radio_id,
                            emlfm->ipc[radio_id].ringbuf_paddr,
                            emlfm->ipc[radio_id].ringbuf_size);
    if (ret)
        goto out;

    emlfm->fw[radio_id].state = WIFI67_FW_STATE_STARTING;

out:
    spin_unlock_irqrestore(&emlfm->lock, flags);
    
    if (!ret) {
        if (!wait_for_completion_timeout(&emlfm->status[radio_id].ready,
                                       WIFI67_FW_READY_TIMEOUT)) {
            ret = -ETIMEDOUT;
        }
    }

    return ret;
}

void wifi67_emlfm_stop_fw(struct wifi67_priv *priv, u8 radio_id)
{
    struct wifi67_emlfm *emlfm = priv->emlfm;
    unsigned long flags;

    if (!emlfm || radio_id >= WIFI67_MAX_RADIOS)
        return;

    spin_lock_irqsave(&emlfm->lock, flags);
    
    if (emlfm->fw[radio_id].state == WIFI67_FW_STATE_READY ||
        emlfm->fw[radio_id].state == WIFI67_FW_STATE_CRASHED) {
        wifi67_hw_stop_fw(priv, radio_id);
        emlfm->fw[radio_id].state = WIFI67_FW_STATE_RESET;
    }
    
    spin_unlock_irqrestore(&emlfm->lock, flags);
}

EXPORT_SYMBOL(wifi67_emlfm_init);
EXPORT_SYMBOL(wifi67_emlfm_deinit);
EXPORT_SYMBOL(wifi67_emlfm_load_fw);
EXPORT_SYMBOL(wifi67_emlfm_start_fw);
EXPORT_SYMBOL(wifi67_emlfm_stop_fw); 