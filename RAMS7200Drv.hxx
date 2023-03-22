/** © Copyright 2022 CERN
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public Licence version 3 (LGPL Version 3),
 * copied verbatim in the file “LICENSE”
 *
 * In applying this licence, CERN does not waive the privileges
 * and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 *
 * Author: Adrien Ledeul (HSE)
 *
 **/

#ifndef RAMS7200DRV_H_
#define RAMS7200DRV_H_

#include <DrvManager.hxx>

class RAMS7200Drv : public DrvManager
{
  public:
    virtual void install_HWMapper();
    virtual void install_HWService();
    virtual void install_AlertService();

    virtual HWObject *getHWObject() const; 
};

#endif