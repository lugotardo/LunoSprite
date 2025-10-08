// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/transaction.h"

#include "app/cmd_transaction.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/document_undo.h"
#include "doc/sprite.h"

namespace app {

using namespace doc;

Transaction::Transaction(Context* ctx, const std::string& label, Modification modification)
  : m_ctx(ctx)
  , m_cmds(NULL)
{
  m_undo = m_ctx->activeDocument()->undoHistory();
  m_cmds = new CmdTransaction(label,
    modification == Modification::ModifyDocument,
    m_undo->savedCounter());

  // Here we are executing an empty CmdTransaction, just to save the
  // SpritePosition. Sub-cmds are executed then one by one, in
  // Transaction::execute()
  m_cmds->execute(m_ctx);
}

Transaction::~Transaction()
{
  try {
    // If it isn't committed, we have to rollback all changes.
    if (m_cmds)
      rollback();
  }
  catch (...) {
    // Just avoid throwing an exception in the dtor (just in case
    // rollback() failed).

    // TODO logging error
  }
}

void Transaction::commit()
{
  ASSERT(m_cmds);

  m_cmds->commit();
  m_undo->add(m_cmds);
  m_cmds = NULL;
}

void Transaction::rollback()
{
  ASSERT(m_cmds);

  m_cmds->undo();

  delete m_cmds;
  m_cmds = NULL;
}

void Transaction::execute(Cmd* cmd)
{
  try {
    cmd->execute(m_ctx);
  }
  catch (...) {
    delete cmd;
    throw;
  }

  try {
    m_cmds->add(cmd);
  }
  catch (...) {
    cmd->undo();
    delete cmd;
    throw;
  }
}

} // namespace app
