/*
 * Copyright (c) 2012      Los Alamos National Security, LLC. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "opal/util/output.h"

#include "orte/runtime/orte_globals.h"

#include "orte/mca/dfs/dfs.h"
#include "orte/mca/dfs/base/base.h"
#include "dfs_app.h"

/*
 * Public string for version number
 */
const char *orte_dfs_app_component_version_string = 
    "ORTE DFS app MCA component version " ORTE_VERSION;

/*
 * Local functionality
 */
static int dfs_app_open(void);
static int dfs_app_close(void);
static int dfs_app_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orte_dfs_base_component_t mca_dfs_app_component =
{
    /* Handle the general mca_component_t struct containing 
     *  meta information about the component
     */
    {
        ORTE_DFS_BASE_VERSION_1_0_0,
        /* Component name and version */
        "app",
        ORTE_MAJOR_VERSION,
        ORTE_MINOR_VERSION,
        ORTE_RELEASE_VERSION,
        
        /* Component open and close functions */
        dfs_app_open,
        dfs_app_close,
        dfs_app_component_query
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int dfs_app_open(void) 
{
    return ORTE_SUCCESS;
}

static int dfs_app_close(void)
{
    return ORTE_SUCCESS;
}

static int dfs_app_component_query(mca_base_module_t **module, int *priority)
{
    if (ORTE_PROC_IS_APP && orte_staged_execution) {
        /* set our priority high as we are the default for apps */
        *priority = 1000;
        *module = (mca_base_module_t *)&orte_dfs_app_module;
        return ORTE_SUCCESS;        
    }
    
    *priority = -1;
    *module = NULL;
    return ORTE_ERROR;
}
