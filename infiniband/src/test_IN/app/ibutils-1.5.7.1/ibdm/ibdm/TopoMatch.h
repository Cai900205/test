/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


// Return 0 if fabrics match 1 otherwise.
// fill in the messages char array with diagnostics..
int
TopoMatchFabrics(
        IBFabric   *p_sFabric,          // The specification fabric
        IBFabric   *p_dFabric,          // The discovered fabric
        const char *anchorNodeName,     // The name of the node to be the anchor point
        int         anchorPortNum,      // The port number of the anchor port
        uint64_t    anchorPortGuid,     // Guid of the anchor port
        char **messages);


// Build a merged fabric from a matched discovered and spec fabrics:
// * Every node from the discovered fabric must appear
// * We sued matched nodes adn system names.
int
TopoMergeDiscAndSpecFabrics(
        IBFabric  *p_sFabric,
        IBFabric  *p_dFabric,
        IBFabric  *p_mFabric);


int
TopoMatchFabricsFromEdge(
        IBFabric *p_sFabric,            // The specification fabric
        IBFabric *p_dFabric,            // The discovered fabric
        char **messages);

