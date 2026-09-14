package com.meijworks.spraycomputerld.service

/** The board's answer to one command line. */
sealed class Reply {
    object Ok : Reply()
    object Busy : Reply()
    data class Error(val reason: String) : Reply()
}
