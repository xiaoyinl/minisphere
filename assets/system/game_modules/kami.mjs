/**
 *  Sphere Runtime for Sphere games
 *  Copyright (c) 2015-2018, Fat Cerberus
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of miniSphere nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
**/

import from from 'from';

export default new
class Kami
{
	constructor(title = Sphere.Game.name)
	{
		this.enabled = SSj.now() > 0;
		this.title = title;
		this.records = [];
		if (this.enabled)
			this.exitJob = Dispatch.onExit(() => this.finish());
	}

	finish()
	{
		SSj.log(`Profiling has completed for "${this.title}"`);
		let sortedRecords = from(this.records)
			.where(it => it.count > 0)
			.descending(it => it.totalTime / it.count);
		for (const record of sortedRecords) {
			let averageTime = Math.round(record.totalTime / record.count / 1000).toLocaleString();
			let count = record.count.toLocaleString();
			let totalTime = Math.round(record.totalTime / 1000).toLocaleString();
			SSj.log(`  ${count} occurrences "${record.description}" took ${totalTime} mcs (avg. ${averageTime} mcs)`);
			record.target[record.methodName] = record.originalFunction;
		}
		this.exitJob.cancel();
	}

	profile(target, methodName, description)
	{
		if (!this.enabled)
			return;

		let originalFunction = target[methodName];
		let record = {
			description: `${target.constructor.name}#${methodName}`,
			count: 0,
			methodName,
			originalFunction,
			target,
			totalTime: 0,
		}
		if (typeof description === 'string')
			record.description = description;

		target[methodName] = function (...args) {
			let startTime = SSj.now();
			originalFunction.apply(this, args);
			let endTime = SSj.now();
			record.totalTime += endTime - startTime;
			++record.count;
		}

		this.records.push(record);
	}
}